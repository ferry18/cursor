#!/usr/bin/env python3
"""
TensorRT INT8 Quantization Conversion for RF-DETR Monochrome Model
Converts RF-DETR model to TensorRT INT8 quantized engine for 3104x2080 monochrome images
"""

import os
import sys
import glob
import numpy as np
import cv2
import torch
import tensorrt as trt
import pycuda.driver as cuda
import pycuda.autoinit
from pathlib import Path
from typing import List, Tuple, Optional
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class MonochromeImageCalibrator(trt.IInt8EntropyCalibrator2):
    """
    INT8 Entropy Calibrator for monochrome images (1 channel)
    Specifically designed for RF-DETR model with 3104x2080 resolution
    """
    
    def __init__(self, calibration_data_dir: str, batch_size: int = 1, 
                 max_calibration_samples: int = 300, cache_file: str = "calibration.cache"):
        """
        Initialize the calibrator
        
        Args:
            calibration_data_dir: Directory containing monochrome PNG images
            batch_size: Batch size for calibration (default 1 for large images)
            max_calibration_samples: Maximum number of calibration samples (using full dataset for better accuracy)
            cache_file: Path to save calibration cache
        """
        super().__init__()
        
        self.batch_size = batch_size
        self.cache_file = cache_file
        self.max_calibration_samples = max_calibration_samples
        
        # Find all PNG images in the calibration directory
        self.image_files = []
        for ext in ['*.png', '*.PNG']:
            self.image_files.extend(glob.glob(os.path.join(calibration_data_dir, ext)))
        
        # Sort for deterministic ordering
        self.image_files.sort()
        
        # Limit to max_calibration_samples (using full dataset for better accuracy)
        self.image_files = self.image_files[:max_calibration_samples]
        
        logger.info(f"Found {len(self.image_files)} calibration images (using {len(self.image_files)})")
        
        # Allocate GPU memory for calibration
        self.device_input = cuda.mem_alloc(self.batch_size * 1 * 2080 * 3104 * np.float32().itemsize)
        
        # Current batch index
        self.current_index = 0
        
        # Load calibration cache if exists
        if os.path.exists(self.cache_file):
            logger.info(f"Loading existing calibration cache: {self.cache_file}")
            with open(self.cache_file, 'rb') as f:
                self.cache = f.read()
        else:
            self.cache = None
    
    def get_batch_size(self):
        """Return the batch size"""
        return self.batch_size
    
    def get_batch(self, names):
        """
        Get the next batch of calibration data
        
        Args:
            names: Names of the inputs (not used in this implementation)
            
        Returns:
            List of GPU memory pointers for the batch
        """
        if self.current_index >= len(self.image_files):
            return None
        
        # Load and preprocess image
        image_path = self.image_files[self.current_index]
        if self.current_index % 5 == 0:  # Log every 5th image to reduce noise
            logger.info(f"Processing calibration image {self.current_index + 1}/{len(self.image_files)}: {os.path.basename(image_path)}")
        
        # Load as monochrome (1 channel)
        image = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
        
        if image is None:
            logger.error(f"Failed to load image: {image_path}")
            self.current_index += 1
            return self.get_batch(names)
        
        # Verify image dimensions
        if image.shape != (2080, 3104):
            logger.warning(f"Image {image_path} has shape {image.shape}, expected (2080, 3104). Resizing...")
            image = cv2.resize(image, (3104, 2080), interpolation=cv2.INTER_LINEAR)
        
        # Normalize to [0, 1] and convert to float32
        image = image.astype(np.float32) / 255.0
        
        # Add batch dimension and channel dimension: (1, 1, H, W)
        image = np.expand_dims(np.expand_dims(image, axis=0), axis=0)
        
        # Copy to GPU memory
        cuda.memcpy_htod(self.device_input, image.astype(np.float32))
        
        self.current_index += 1
        return [self.device_input]
    
    def read_calibration_cache(self):
        """Read calibration cache"""
        return self.cache
    
    def write_calibration_cache(self, cache):
        """Write calibration cache"""
        with open(self.cache_file, 'wb') as f:
            f.write(cache)
        logger.info(f"Calibration cache saved to: {self.cache_file}")

def export_rfdetr_to_onnx(checkpoint_path: str, output_path: str = "rfdetr_monochrome.onnx"):
    """
    Export RF-DETR model to ONNX format for monochrome images
    
    Args:
        checkpoint_path: Path to the trained model checkpoint
        output_path: Output ONNX file path
    """
    logger.info("Exporting RF-DETR model to ONNX...")
    
    try:
        from rfdetr import RFDETRNano
        
        # Load the model with checkpoint
        model = RFDETRNano(pretrain_weights=checkpoint_path)
        
        # Export to ONNX
        model.export(output_path=output_path)
        
        logger.info(f"ONNX model exported to: {output_path}")
        return output_path
        
    except Exception as e:
        logger.error(f"Failed to export model to ONNX: {e}")
        raise

def build_tensorrt_engine(onnx_path: str, engine_path: str, 
                         calibration_data_dir: str, 
                         precision: str = "INT8",
                         workspace_size: int = 4 * 1024 * 1024 * 1024,  # 4GB
                         max_calibration_samples: int = 300):
    """
    Build TensorRT engine from ONNX model
    
    Args:
        onnx_path: Path to ONNX model
        engine_path: Output TensorRT engine path
        calibration_data_dir: Directory containing calibration images
        precision: Precision mode ("FP32", "FP16", "INT8")
        workspace_size: Maximum workspace size in bytes
    """
    logger.info(f"Building TensorRT engine with {precision} precision...")
    
    # Create TensorRT logger
    logger_trt = trt.Logger(trt.Logger.WARNING)
    
    # Create builder
    builder = trt.Builder(logger_trt)
    config = builder.create_builder_config()
    
    # Set workspace size (new API)
    config.set_memory_pool_limit(trt.MemoryPoolType.WORKSPACE, workspace_size)
    
    # Enable precision modes
    if precision == "FP16" and builder.platform_has_fast_fp16:
        config.set_flag(trt.BuilderFlag.FP16)
        logger.info("FP16 precision enabled")
    elif precision == "INT8":
        config.set_flag(trt.BuilderFlag.INT8)
        logger.info("INT8 precision enabled")
        
        # Create calibrator for INT8
        calibrator = MonochromeImageCalibrator(
            calibration_data_dir=calibration_data_dir,
            batch_size=1,
            max_calibration_samples=max_calibration_samples,  # Use all available images for calibration
            cache_file="rfdetr_int8_calibration.cache"
        )
        config.int8_calibrator = calibrator
    
    # Parse ONNX model
    network = builder.create_network(1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH))
    parser = trt.OnnxParser(network, logger_trt)
    
    with open(onnx_path, 'rb') as model:
        if not parser.parse(model.read()):
            logger.error("Failed to parse ONNX model")
            for error in range(parser.num_errors):
                logger.error(parser.get_error(error))
            raise RuntimeError("ONNX parsing failed")
    
    logger.info("ONNX model parsed successfully")
    
    # Build engine
    engine_data = builder.build_serialized_network(network, config)
    
    if engine_data is None:
        logger.error("Failed to build TensorRT engine")
        raise RuntimeError("Engine building failed")
    
    # Save engine
    with open(engine_path, 'wb') as f:
        f.write(engine_data)
    
    logger.info(f"TensorRT engine saved to: {engine_path}")
    
    # Print engine info
    logger.info(f"Engine size: {os.path.getsize(engine_path) / (1024*1024):.2f} MB")
    
    return engine_path

def test_tensorrt_engine(engine_path: str, test_image_path: str):
    """
    Test the TensorRT engine with a sample image
    
    Args:
        engine_path: Path to TensorRT engine
        test_image_path: Path to test image
    """
    logger.info("Testing TensorRT engine...")
    
    # Load engine
    logger_trt = trt.Logger(trt.Logger.WARNING)
    runtime = trt.Runtime(logger_trt)
    
    with open(engine_path, 'rb') as f:
        engine = runtime.deserialize_cuda_engine(f.read())
    
    # Create execution context
    context = engine.create_execution_context()
    
    # Load and preprocess test image
    image = cv2.imread(test_image_path, cv2.IMREAD_GRAYSCALE)
    if image.shape != (2080, 3104):
        image = cv2.resize(image, (3104, 2080), interpolation=cv2.INTER_LINEAR)
    
    # Resize to match engine input size (384x384)
    image = cv2.resize(image, (384, 384), interpolation=cv2.INTER_LINEAR)
    
    # Convert to 3-channel by repeating the single channel
    image = np.stack([image, image, image], axis=2)  # (H, W, 3)
    
    # Normalize
    image = image.astype(np.float32) / 255.0
    image = np.transpose(image, (2, 0, 1))  # (3, H, W)
    image = np.expand_dims(image, axis=0)  # (1, 3, H, W)
    image = np.ascontiguousarray(image)  # Make array contiguous
    
    # Get binding information (new API)
    num_bindings = engine.num_io_tensors
    logger.info(f"Number of bindings: {num_bindings}")
    
    for i in range(num_bindings):
        binding_name = engine.get_tensor_name(i)
        binding_shape = engine.get_tensor_shape(binding_name)
        binding_dtype = engine.get_tensor_dtype(binding_name)
        logger.info(f"Binding {i}: {binding_name}, shape: {binding_shape}, dtype: {binding_dtype}")
    
    # Allocate GPU memory
    input_size = image.nbytes
    # Calculate output sizes based on binding shapes (new API)
    dets_name = engine.get_tensor_name(1)
    labels_name = engine.get_tensor_name(2)
    dets_shape = engine.get_tensor_shape(dets_name)
    labels_shape = engine.get_tensor_shape(labels_name)
    dets_size = int(np.prod(dets_shape) * 4)  # Assuming float32 (4 bytes)
    labels_size = int(np.prod(labels_shape) * 4)  # Assuming float32 (4 bytes)
    
    d_input = cuda.mem_alloc(input_size)
    d_dets = cuda.mem_alloc(dets_size)
    d_labels = cuda.mem_alloc(labels_size)
    
    # Create CUDA stream
    stream = cuda.Stream()
    
    # Set tensor addresses
    context.set_tensor_address("input", int(d_input))
    context.set_tensor_address("dets", int(d_dets))
    context.set_tensor_address("labels", int(d_labels))
    
    # Copy input to GPU
    cuda.memcpy_htod_async(d_input, image, stream)
    
    # Execute inference (new API)
    context.execute_async_v3(stream_handle=stream.handle)
    
    # Copy outputs from GPU
    dets_output = np.empty(dets_size // 4, dtype=np.float32)  # Assuming float32 output
    labels_output = np.empty(labels_size // 4, dtype=np.float32)  # Assuming float32 output
    cuda.memcpy_dtoh_async(dets_output, d_dets, stream)
    cuda.memcpy_dtoh_async(labels_output, d_labels, stream)
    
    # Synchronize
    stream.synchronize()
    
    logger.info("TensorRT inference completed successfully")
    logger.info(f"Dets output shape: {dets_output.shape}")
    logger.info(f"Dets output range: [{dets_output.min():.6f}, {dets_output.max():.6f}]")
    logger.info(f"Labels output shape: {labels_output.shape}")
    logger.info(f"Labels output range: [{labels_output.min():.6f}, {labels_output.max():.6f}]")
    
    return dets_output, labels_output

def main():
    """Main conversion function"""
    # Configuration
    checkpoint_path = "/home/agx/ethercat/training/output/checkpoint_best_regular.pth"
    quantization_dataset_dir = "/home/agx/ethercat/training/quantization_dataset/test"
    onnx_output_path = "output/inference_model.onnx"  # Use the actual RF-DETR output path
    engine_output_path = "rfdetr_monochrome_int8.engine"
    
    # Calibration settings (using full dataset for better accuracy)
    max_calibration_samples = 300
    
    # Verify inputs
    if not os.path.exists(checkpoint_path):
        logger.error(f"Checkpoint not found: {checkpoint_path}")
        return 1
    
    if not os.path.exists(quantization_dataset_dir):
        logger.error(f"Quantization dataset not found: {quantization_dataset_dir}")
        return 1
    
    try:
        # Step 1: Export to ONNX
        logger.info("=" * 60)
        logger.info("STEP 1: Exporting RF-DETR to ONNX")
        logger.info("=" * 60)
        
        onnx_path = export_rfdetr_to_onnx(checkpoint_path, onnx_output_path)
        
        # Step 2: Build TensorRT INT8 Engine
        logger.info("=" * 60)
        logger.info("STEP 2: Building TensorRT INT8 Engine")
        logger.info("=" * 60)
        
        engine_path = build_tensorrt_engine(
            onnx_path=onnx_path,
            engine_path=engine_output_path,
            calibration_data_dir=quantization_dataset_dir,
            precision="INT8",
            workspace_size=4 * 1024 * 1024 * 1024,  # 4GB
            max_calibration_samples=max_calibration_samples
        )
        
        # Step 3: Test the engine
        logger.info("=" * 60)
        logger.info("STEP 3: Testing TensorRT Engine")
        logger.info("=" * 60)
        
        # Find a test image
        test_images = glob.glob(os.path.join(quantization_dataset_dir, "*.png"))
        if test_images:
            test_image = test_images[0]
            test_tensorrt_engine(engine_path, test_image)
        
        logger.info("=" * 60)
        logger.info("CONVERSION COMPLETED SUCCESSFULLY!")
        logger.info("=" * 60)
        logger.info(f"ONNX Model: {onnx_path}")
        logger.info(f"TensorRT Engine: {engine_path}")
        logger.info(f"Calibration Cache: rfdetr_int8_calibration.cache")
        logger.info("=" * 60)
        
        return 0
        
    except Exception as e:
        logger.error(f"Conversion failed: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())
