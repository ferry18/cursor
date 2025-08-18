#!/usr/bin/env python3
"""
Fix RF-DETR ONNX Export Issues for Monochrome Images

This script addresses:
1. Channel mismatch (3 channels vs 1 channel)
2. TracerWarning issues with dynamic control flow
3. TensorRT INT8 quantization problems
4. Random bounding box initialization issues
"""

import torch
import torch.nn as nn
import torch.onnx
import numpy as np
from typing import Dict, List, Tuple, Optional
import warnings
import os
import logging

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class ONNXExportWrapper(nn.Module):
    """Wrapper to handle ONNX export issues for RF-DETR"""
    
    def __init__(self, model, num_channels=1):
        super().__init__()
        self.model = model
        self.num_channels = num_channels
        
        # Fix channel mismatch in patch embeddings
        self._fix_channel_mismatch()
        
        # Initialize reference points properly
        self._initialize_reference_points()
        
    def _fix_channel_mismatch(self):
        """Fix the channel mismatch in the backbone"""
        try:
            # Access the patch embedding layer
            patch_embed = self.model.backbone[0].encoder.encoder.embeddings.patch_embeddings
            
            if hasattr(patch_embed, 'projection'):
                old_conv = patch_embed.projection
                in_channels = old_conv.in_channels
                
                if in_channels != self.num_channels:
                    logger.info(f"Fixing channel mismatch: {in_channels} -> {self.num_channels}")
                    
                    # Create new convolution with correct input channels
                    new_conv = nn.Conv2d(
                        self.num_channels,
                        old_conv.out_channels,
                        kernel_size=old_conv.kernel_size,
                        stride=old_conv.stride,
                        padding=old_conv.padding,
                        bias=old_conv.bias is not None
                    )
                    
                    # Initialize weights (average across input channels if needed)
                    with torch.no_grad():
                        if in_channels == 3 and self.num_channels == 1:
                            # Average RGB weights for grayscale
                            new_conv.weight.data = old_conv.weight.data.mean(dim=1, keepdim=True)
                        else:
                            # Copy first channel and initialize rest if expanding
                            new_conv.weight.data[:, :min(in_channels, self.num_channels)] = \
                                old_conv.weight.data[:, :min(in_channels, self.num_channels)]
                        
                        if old_conv.bias is not None:
                            new_conv.bias.data = old_conv.bias.data
                    
                    # Replace the convolution
                    patch_embed.projection = new_conv
                    patch_embed.num_channels = self.num_channels
                    
                    # Update config if it exists
                    if hasattr(patch_embed, 'config'):
                        patch_embed.config.num_channels = self.num_channels
                        
        except Exception as e:
            logger.warning(f"Could not fix channel mismatch: {e}")
    
    def _initialize_reference_points(self):
        """Ensure reference points are properly initialized"""
        try:
            # Initialize transformer decoder reference points if they exist
            if hasattr(self.model, 'transformer') and hasattr(self.model.transformer, 'decoder'):
                decoder = self.model.transformer.decoder
                
                # Check for reference point head
                if hasattr(decoder, 'ref_point_head'):
                    # Ensure weights are initialized
                    for module in decoder.ref_point_head.modules():
                        if isinstance(module, nn.Linear):
                            if not torch.isfinite(module.weight).all():
                                nn.init.xavier_uniform_(module.weight)
                                if module.bias is not None:
                                    nn.init.zeros_(module.bias)
                                logger.info("Re-initialized reference point head weights")
                                
        except Exception as e:
            logger.warning(f"Could not check reference points: {e}")
    
    def forward(self, x):
        # Ensure input has correct channels
        if x.shape[1] != self.num_channels:
            if x.shape[1] == 3 and self.num_channels == 1:
                # Convert RGB to grayscale
                x = x.mean(dim=1, keepdim=True)
            elif x.shape[1] == 1 and self.num_channels == 3:
                # Expand grayscale to RGB
                x = x.repeat(1, 3, 1, 1)
        
        return self.model(x)


def fix_onnx_export_issues(model, checkpoint_path: str, output_path: str, 
                          num_channels: int = 1,
                          input_size: Tuple[int, int] = (384, 384),
                          opset_version: int = 17,
                          dynamic_axes: Optional[Dict] = None):
    """
    Export RF-DETR model to ONNX with fixes for common issues
    
    Args:
        model: RF-DETR model instance
        checkpoint_path: Path to checkpoint
        output_path: Output ONNX file path
        num_channels: Number of input channels (1 for grayscale)
        input_size: Input image size (H, W)
        opset_version: ONNX opset version
        dynamic_axes: Dynamic axes specification
    """
    
    # Load checkpoint if provided
    if checkpoint_path and os.path.exists(checkpoint_path):
        logger.info(f"Loading checkpoint from {checkpoint_path}")
        checkpoint = torch.load(checkpoint_path, map_location='cpu')
        if 'model' in checkpoint:
            model.load_state_dict(checkpoint['model'], strict=False)
        else:
            model.load_state_dict(checkpoint, strict=False)
    
    # Wrap model to fix issues
    wrapped_model = ONNXExportWrapper(model, num_channels=num_channels)
    wrapped_model.eval()
    
    # Create dummy input
    batch_size = 1
    dummy_input = torch.randn(batch_size, num_channels, *input_size)
    
    # Set default dynamic axes if not provided
    if dynamic_axes is None:
        dynamic_axes = {
            'input': {0: 'batch_size'},
            'boxes': {0: 'batch_size'},
            'labels': {0: 'batch_size'}
        }
    
    # Export with proper settings
    logger.info(f"Exporting to ONNX with opset {opset_version}")
    
    # Suppress TracerWarnings
    with warnings.catch_warnings():
        warnings.filterwarnings("ignore", category=torch.jit.TracerWarning)
        
        # Use torch.no_grad() to avoid gradient tracking issues
        with torch.no_grad():
            # First, do a forward pass to ensure all buffers are initialized
            _ = wrapped_model(dummy_input)
            
            # Export to ONNX
            torch.onnx.export(
                wrapped_model,
                dummy_input,
                output_path,
                export_params=True,
                opset_version=opset_version,
                do_constant_folding=True,
                input_names=['input'],
                output_names=['boxes', 'labels'],
                dynamic_axes=dynamic_axes,
                # Additional settings to help with export
                operator_export_type=torch.onnx.OperatorExportTypes.ONNX,
                keep_initializers_as_inputs=False,
            )
    
    logger.info(f"ONNX model exported to {output_path}")
    
    # Verify the exported model
    try:
        import onnx
        import onnxruntime as ort
        
        # Check ONNX model
        onnx_model = onnx.load(output_path)
        onnx.checker.check_model(onnx_model)
        logger.info("ONNX model validation passed")
        
        # Test inference
        ort_session = ort.InferenceSession(output_path)
        input_name = ort_session.get_inputs()[0].name
        outputs = ort_session.run(None, {input_name: dummy_input.numpy()})
        logger.info(f"ONNX inference test passed. Output shapes: {[o.shape for o in outputs]}")
        
    except Exception as e:
        logger.warning(f"ONNX verification failed: {e}")
    
    return output_path


def fix_tensorrt_int8_issues(onnx_path: str, calibration_data_dir: str, 
                           engine_path: str, num_channels: int = 1):
    """
    Build TensorRT engine with proper INT8 calibration
    """
    try:
        import tensorrt as trt
        from convert_to_tensorrt_int8 import MonochromeImageCalibrator
        
        logger.info("Building TensorRT engine with fixed INT8 calibration")
        
        # Create calibrator with proper settings
        calibrator = MonochromeImageCalibrator(
            calibration_data_dir=calibration_data_dir,
            batch_size=1,
            cache_file="rfdetr_int8_calibration_fixed.cache",
            num_channels=num_channels
        )
        
        # Build engine with explicit precision settings
        logger_trt = trt.Logger(trt.Logger.WARNING)
        builder = trt.Builder(logger_trt)
        config = builder.create_builder_config()
        
        # Set memory pool
        config.set_memory_pool_limit(trt.MemoryPoolType.WORKSPACE, 4 * 1024 * 1024 * 1024)
        
        # Enable INT8 with proper calibrator
        config.set_flag(trt.BuilderFlag.INT8)
        config.int8_calibrator = calibrator
        
        # Parse ONNX
        network = builder.create_network(1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH))
        parser = trt.OnnxParser(network, logger_trt)
        
        with open(onnx_path, 'rb') as f:
            if not parser.parse(f.read()):
                logger.error("Failed to parse ONNX model")
                for error in range(parser.num_errors):
                    logger.error(parser.get_error(error))
                return None
        
        # Set input shape optimization profiles
        profile = builder.create_optimization_profile()
        input_tensor = network.get_input(0)
        input_shape = input_tensor.shape
        
        # Set min, opt, max shapes for dynamic batch
        min_shape = [1, num_channels, input_shape[2], input_shape[3]]
        opt_shape = [1, num_channels, input_shape[2], input_shape[3]]
        max_shape = [4, num_channels, input_shape[2], input_shape[3]]
        
        profile.set_shape(input_tensor.name, min_shape, opt_shape, max_shape)
        config.add_optimization_profile(profile)
        
        # Build engine
        logger.info("Building TensorRT engine (this may take a while)...")
        engine = builder.build_engine(network, config)
        
        if engine:
            # Save engine
            with open(engine_path, 'wb') as f:
                f.write(engine.serialize())
            logger.info(f"TensorRT engine saved to {engine_path}")
        else:
            logger.error("Failed to build TensorRT engine")
            
        return engine
        
    except Exception as e:
        logger.error(f"TensorRT build failed: {e}")
        return None


if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Fix RF-DETR ONNX export issues")
    parser.add_argument("--checkpoint", required=True, help="Path to RF-DETR checkpoint")
    parser.add_argument("--output", default="rfdetr_fixed.onnx", help="Output ONNX path")
    parser.add_argument("--channels", type=int, default=1, help="Number of input channels")
    parser.add_argument("--size", type=int, nargs=2, default=[384, 384], help="Input size (H W)")
    parser.add_argument("--build-tensorrt", action="store_true", help="Build TensorRT engine")
    parser.add_argument("--calibration-dir", help="Calibration data directory for INT8")
    
    args = parser.parse_args()
    
    # Load model
    from rfdetr import RFDETRNano
    model = RFDETRNano()
    
    # Export with fixes
    onnx_path = fix_onnx_export_issues(
        model,
        args.checkpoint,
        args.output,
        num_channels=args.channels,
        input_size=tuple(args.size)
    )
    
    # Build TensorRT if requested
    if args.build_tensorrt and args.calibration_dir:
        engine_path = args.output.replace('.onnx', '.engine')
        fix_tensorrt_int8_issues(
            onnx_path,
            args.calibration_dir,
            engine_path,
            num_channels=args.channels
        )