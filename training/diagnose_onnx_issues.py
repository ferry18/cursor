#!/usr/bin/env python3
"""
Diagnostic script for RF-DETR ONNX export issues

This script helps identify:
1. Uninitialized tensors causing random bounding boxes
2. Dynamic control flow issues
3. Input preprocessing mismatches
"""

import torch
import numpy as np
import onnx
import onnxruntime as ort
from typing import List, Dict, Tuple
import cv2
import logging
import json

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class ONNXModelDiagnostics:
    def __init__(self, onnx_path: str):
        self.onnx_path = onnx_path
        self.ort_session = ort.InferenceSession(onnx_path)
        self.input_name = self.ort_session.get_inputs()[0].name
        self.input_shape = self.ort_session.get_inputs()[0].shape
        self.output_names = [o.name for o in self.ort_session.get_outputs()]
        
    def check_model_info(self):
        """Display model information"""
        logger.info("=== ONNX Model Information ===")
        logger.info(f"Input name: {self.input_name}")
        logger.info(f"Input shape: {self.input_shape}")
        logger.info(f"Output names: {self.output_names}")
        
        # Check for dynamic dimensions
        dynamic_dims = []
        for i, dim in enumerate(self.input_shape):
            if isinstance(dim, str) or dim == -1:
                dynamic_dims.append(i)
        if dynamic_dims:
            logger.warning(f"Dynamic dimensions found at indices: {dynamic_dims}")
            
    def test_determinism(self, num_runs: int = 5):
        """Test if model outputs are deterministic"""
        logger.info("\n=== Testing Model Determinism ===")
        
        # Create fixed input
        if any(isinstance(d, str) or d == -1 for d in self.input_shape):
            # Handle dynamic shapes
            shape = [1 if isinstance(d, str) or d == -1 else d for d in self.input_shape]
            shape[0] = 1  # batch size
            if len(shape) == 4:  # Assuming NCHW format
                shape[1] = 1  # channels
                shape[2] = shape[3] = 384  # spatial dims
        else:
            shape = list(self.input_shape)
            
        fixed_input = np.ones(shape, dtype=np.float32) * 0.5
        
        outputs = []
        for i in range(num_runs):
            output = self.ort_session.run(None, {self.input_name: fixed_input})
            outputs.append(output)
            
        # Check consistency
        is_deterministic = True
        for i in range(1, num_runs):
            for j, (out_prev, out_curr) in enumerate(zip(outputs[0], outputs[i])):
                if not np.allclose(out_prev, out_curr, rtol=1e-5):
                    is_deterministic = False
                    diff = np.abs(out_prev - out_curr).max()
                    logger.error(f"Non-deterministic output {j}: max diff = {diff}")
                    
                    # Check for extremely large values (uninitialized memory)
                    if np.abs(out_curr).max() > 1e6:
                        logger.error(f"Extremely large values detected (possibly uninitialized): max = {np.abs(out_curr).max()}")
                        
        if is_deterministic:
            logger.info("✓ Model outputs are deterministic")
        else:
            logger.error("✗ Model outputs are NOT deterministic - this explains random bounding boxes!")
            
        return is_deterministic
    
    def test_output_ranges(self):
        """Test output value ranges with different inputs"""
        logger.info("\n=== Testing Output Value Ranges ===")
        
        # Prepare test inputs
        shape = [1, 1, 384, 384]  # Assuming monochrome input
        test_inputs = {
            'zeros': np.zeros(shape, dtype=np.float32),
            'ones': np.ones(shape, dtype=np.float32),
            'random': np.random.randn(*shape).astype(np.float32),
            'normalized': np.random.randn(*shape).astype(np.float32) * 0.229 + 0.485  # ImageNet-like
        }
        
        for name, test_input in test_inputs.items():
            logger.info(f"\nTesting with {name} input:")
            outputs = self.ort_session.run(None, {self.input_name: test_input})
            
            for i, output in enumerate(outputs):
                logger.info(f"  Output {i} shape: {output.shape}")
                logger.info(f"  Output {i} range: [{output.min():.4f}, {output.max():.4f}]")
                logger.info(f"  Output {i} mean: {output.mean():.4f}, std: {output.std():.4f}")
                
                # Check for suspicious values
                if np.isnan(output).any():
                    logger.error(f"  ✗ NaN values detected in output {i}!")
                if np.isinf(output).any():
                    logger.error(f"  ✗ Inf values detected in output {i}!")
                if output.max() > 1e6 or output.min() < -1e6:
                    logger.warning(f"  ⚠ Very large values detected in output {i}")
                    
    def analyze_first_detection(self):
        """Analyze the behavior of first few detections"""
        logger.info("\n=== Analyzing First Detection Behavior ===")
        
        # Run inference multiple times
        shape = [1, 1, 384, 384]
        test_input = np.random.randn(*shape).astype(np.float32) * 0.1
        
        num_runs = 10
        first_box_history = []
        first_score_history = []
        
        for i in range(num_runs):
            outputs = self.ort_session.run(None, {self.input_name: test_input})
            
            # Assuming outputs are [boxes, scores/labels]
            boxes = outputs[0]
            scores = outputs[1] if len(outputs) > 1 else None
            
            if boxes.shape[0] > 0 and boxes.shape[1] > 0:
                first_box = boxes[0, 0, :4] if len(boxes.shape) == 3 else boxes[0, :4]
                first_box_history.append(first_box)
                
                if scores is not None:
                    first_score = scores[0, 0] if len(scores.shape) > 1 else scores[0]
                    first_score_history.append(first_score)
        
        # Analyze variation
        if first_box_history:
            first_box_history = np.array(first_box_history)
            box_std = np.std(first_box_history, axis=0)
            logger.info(f"First box coordinate std: {box_std}")
            
            if (box_std > 100).any():  # Large variation indicates instability
                logger.error("✗ High variation in first detection box - indicates initialization issue!")
            else:
                logger.info("✓ First detection box is relatively stable")
                
    def suggest_fixes(self):
        """Suggest fixes based on diagnostics"""
        logger.info("\n=== Suggested Fixes ===")
        
        suggestions = [
            "1. Ensure all model weights are properly initialized before export",
            "2. Add explicit initialization for query embeddings and reference points",
            "3. Use torch.no_grad() context during export",
            "4. Check for any conditional logic that depends on tensor values",
            "5. Verify input preprocessing matches training preprocessing",
            "6. Consider using ONNX opset 17 or higher for better operator support",
            "7. Test with different batch sizes to identify batch-related issues"
        ]
        
        for suggestion in suggestions:
            logger.info(f"  {suggestion}")


def diagnose_onnx_model(onnx_path: str):
    """Run comprehensive diagnostics on ONNX model"""
    diag = ONNXModelDiagnostics(onnx_path)
    
    diag.check_model_info()
    is_deterministic = diag.test_determinism()
    diag.test_output_ranges()
    diag.analyze_first_detection()
    diag.suggest_fixes()
    
    return is_deterministic


def compare_pytorch_onnx(pytorch_model, onnx_path: str, input_shape: Tuple[int, ...] = (1, 1, 384, 384)):
    """Compare PyTorch and ONNX model outputs"""
    logger.info("\n=== Comparing PyTorch vs ONNX Outputs ===")
    
    # Create test input
    test_input = torch.randn(*input_shape)
    
    # PyTorch inference
    pytorch_model.eval()
    with torch.no_grad():
        pytorch_output = pytorch_model(test_input)
    
    # ONNX inference
    ort_session = ort.InferenceSession(onnx_path)
    input_name = ort_session.get_inputs()[0].name
    onnx_output = ort_session.run(None, {input_name: test_input.numpy()})
    
    # Compare outputs
    if isinstance(pytorch_output, dict):
        # Handle dict output
        for key in ['pred_boxes', 'pred_logits']:
            if key in pytorch_output:
                pytorch_tensor = pytorch_output[key].numpy()
                # Find corresponding ONNX output
                for i, onnx_tensor in enumerate(onnx_output):
                    if onnx_tensor.shape == pytorch_tensor.shape:
                        diff = np.abs(pytorch_tensor - onnx_tensor).max()
                        logger.info(f"{key}: max diff = {diff}")
                        if diff > 1e-3:
                            logger.warning(f"Large difference in {key}!")
                        break
    else:
        # Handle tuple/list output
        for i, (pt_out, onnx_out) in enumerate(zip(pytorch_output, onnx_output)):
            if isinstance(pt_out, torch.Tensor):
                diff = np.abs(pt_out.numpy() - onnx_out).max()
                logger.info(f"Output {i}: max diff = {diff}")
                if diff > 1e-3:
                    logger.warning(f"Large difference in output {i}!")


if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Diagnose RF-DETR ONNX issues")
    parser.add_argument("--onnx", required=True, help="Path to ONNX model")
    parser.add_argument("--checkpoint", help="Path to PyTorch checkpoint for comparison")
    
    args = parser.parse_args()
    
    # Run diagnostics
    is_deterministic = diagnose_onnx_model(args.onnx)
    
    # Compare with PyTorch if checkpoint provided
    if args.checkpoint:
        try:
            from rfdetr import RFDETRNano
            model = RFDETRNano(pretrain_weights=args.checkpoint)
            compare_pytorch_onnx(model, args.onnx)
        except Exception as e:
            logger.error(f"Could not compare with PyTorch: {e}")
    
    # Final verdict
    if not is_deterministic:
        logger.error("\n❌ Model has non-deterministic behavior - this is the root cause of random bounding boxes!")
        logger.info("Run the fix_rfdetr_onnx_export.py script to resolve these issues.")
    else:
        logger.info("\n✅ Model appears to be deterministic. Random boxes might be due to other factors.")