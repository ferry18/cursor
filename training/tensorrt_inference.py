#!/usr/bin/env python3
"""
TensorRT Inference for RF-DETR Monochrome Model
Provides inference interface for the INT8 quantized TensorRT engine
"""

import os
import sys
import numpy as np
import cv2
import tensorrt as trt
import pycuda.driver as cuda
import pycuda.autoinit
import time
from typing import List, Tuple, Optional, Dict, Any
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class RFDETRTensorRTInference:
    """
    TensorRT inference class for RF-DETR monochrome model
    """
    
    def __init__(self, engine_path: str):
        """
        Initialize TensorRT inference
        
        Args:
            engine_path: Path to TensorRT engine file
        """
        self.engine_path = engine_path
        self.engine = None
        self.context = None
        self.stream = None
        self.d_input = None
        self.d_output = None
        self.host_input = None
        self.host_output = None
        
        # Load engine
        self._load_engine()
        self._create_context()
        
        # Model configuration
        self.input_shape = (1, 1, 2080, 3104)  # (batch, channels, height, width)
        self.input_size = np.prod(self.input_shape) * np.float32().itemsize
        
    def _load_engine(self):
        """Load TensorRT engine"""
        logger.info(f"Loading TensorRT engine from: {self.engine_path}")
        
        if not os.path.exists(self.engine_path):
            raise FileNotFoundError(f"Engine file not found: {self.engine_path}")
        
        logger_trt = trt.Logger(trt.Logger.WARNING)
        runtime = trt.Runtime(logger_trt)
        
        with open(self.engine_path, 'rb') as f:
            self.engine = runtime.deserialize_cuda_engine(f.read())
        
        logger.info("TensorRT engine loaded successfully")
        
    def _create_context(self):
        """Create execution context and allocate memory"""
        self.context = self.engine.create_execution_context()
        self.stream = cuda.Stream()
        
        # Allocate GPU memory
        self.d_input = cuda.mem_alloc(self.input_size)
        output_size = self.engine.get_binding_size(1)  # Assuming output is binding 1
        self.d_output = cuda.mem_alloc(output_size)
        
        # Allocate host memory
        self.host_input = cuda.pagelocked_empty(self.input_shape, dtype=np.float32)
        self.host_output = cuda.pagelocked_empty(output_size // 4, dtype=np.float32)
        
        logger.info("TensorRT context and memory allocated")
        
    def preprocess_image(self, image_path: str) -> np.ndarray:
        """
        Preprocess monochrome image for inference
        
        Args:
            image_path: Path to input image
            
        Returns:
            Preprocessed image as numpy array
        """
        # Load as monochrome (1 channel)
        image = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
        
        if image is None:
            raise ValueError(f"Failed to load image: {image_path}")
        
        # Verify and resize if necessary
        if image.shape != (2080, 3104):
            logger.warning(f"Image shape {image.shape} != (2080, 3104), resizing...")
            image = cv2.resize(image, (3104, 2080), interpolation=cv2.INTER_LINEAR)
        
        # Normalize to [0, 1]
        image = image.astype(np.float32) / 255.0
        
        # Add batch and channel dimensions: (1, 1, H, W)
        image = np.expand_dims(np.expand_dims(image, axis=0), axis=0)
        
        return image
        
    def preprocess_array(self, image: np.ndarray) -> np.ndarray:
        """
        Preprocess numpy array for inference
        
        Args:
            image: Input image as numpy array (H, W) or (1, H, W)
            
        Returns:
            Preprocessed image as numpy array
        """
        # Ensure 2D array
        if image.ndim == 3 and image.shape[0] == 1:
            image = image.squeeze(0)
        elif image.ndim != 2:
            raise ValueError(f"Expected 2D array, got shape {image.shape}")
        
        # Verify dimensions
        if image.shape != (2080, 3104):
            logger.warning(f"Image shape {image.shape} != (2080, 3104), resizing...")
            image = cv2.resize(image, (3104, 2080), interpolation=cv2.INTER_LINEAR)
        
        # Normalize to [0, 1]
        image = image.astype(np.float32) / 255.0
        
        # Add batch and channel dimensions: (1, 1, H, W)
        image = np.expand_dims(np.expand_dims(image, axis=0), axis=0)
        
        return image
        
    def infer(self, image: np.ndarray) -> np.ndarray:
        """
        Run inference on preprocessed image
        
        Args:
            image: Preprocessed image array (1, 1, H, W)
            
        Returns:
            Model output as numpy array
        """
        # Verify input shape
        if image.shape != self.input_shape:
            raise ValueError(f"Expected input shape {self.input_shape}, got {image.shape}")
        
        # Copy input to host memory
        np.copyto(self.host_input, image)
        
        # Copy input to GPU
        cuda.memcpy_htod_async(self.d_input, self.host_input, self.stream)
        
        # Execute inference
        self.context.execute_async_v2(
            bindings=[int(self.d_input), int(self.d_output)], 
            stream_handle=self.stream.handle
        )
        
        # Copy output from GPU
        cuda.memcpy_dtoh_async(self.host_output, self.d_output, self.stream)
        
        # Synchronize
        self.stream.synchronize()
        
        return self.host_output.copy()
        
    def infer_from_path(self, image_path: str) -> np.ndarray:
        """
        Run inference on image from file path
        
        Args:
            image_path: Path to input image
            
        Returns:
            Model output as numpy array
        """
        # Preprocess image
        image = self.preprocess_image(image_path)
        
        # Run inference
        return self.infer(image)
        
    def benchmark(self, image_path: str, num_runs: int = 100) -> Dict[str, float]:
        """
        Benchmark inference performance
        
        Args:
            image_path: Path to test image
            num_runs: Number of inference runs for benchmarking
            
        Returns:
            Dictionary with timing statistics
        """
        logger.info(f"Benchmarking with {num_runs} runs...")
        
        # Preprocess image once
        image = self.preprocess_image(image_path)
        
        # Warmup
        for _ in range(10):
            self.infer(image)
        
        # Benchmark
        times = []
        for i in range(num_runs):
            start_time = time.time()
            self.infer(image)
            end_time = time.time()
            times.append((end_time - start_time) * 1000)  # Convert to ms
            
            if (i + 1) % 20 == 0:
                logger.info(f"Completed {i + 1}/{num_runs} runs")
        
        # Calculate statistics
        times = np.array(times)
        stats = {
            'mean_ms': float(np.mean(times)),
            'std_ms': float(np.std(times)),
            'min_ms': float(np.min(times)),
            'max_ms': float(np.max(times)),
            'median_ms': float(np.median(times)),
            'fps': 1000.0 / float(np.mean(times))
        }
        
        logger.info(f"Benchmark results:")
        logger.info(f"  Mean: {stats['mean_ms']:.2f} ms")
        logger.info(f"  Std:  {stats['std_ms']:.2f} ms")
        logger.info(f"  Min:  {stats['min_ms']:.2f} ms")
        logger.info(f"  Max:  {stats['max_ms']:.2f} ms")
        logger.info(f"  FPS:  {stats['fps']:.2f}")
        
        return stats
        
    def __del__(self):
        """Cleanup resources"""
        if hasattr(self, 'stream') and self.stream:
            self.stream.synchronize()

def main():
    """Example usage and testing"""
    # Configuration
    engine_path = "rfdetr_monochrome_int8.engine"
    test_image_path = "quantization_dataset/test/IMG_1151_crop0_0.png"
    
    if not os.path.exists(engine_path):
        logger.error(f"Engine file not found: {engine_path}")
        logger.info("Please run convert_to_tensorrt_int8.py first to create the engine")
        return 1
    
    if not os.path.exists(test_image_path):
        logger.error(f"Test image not found: {test_image_path}")
        return 1
    
    try:
        # Initialize inference
        logger.info("Initializing TensorRT inference...")
        inferencer = RFDETRTensorRTInference(engine_path)
        
        # Test inference
        logger.info("Running test inference...")
        output = inferencer.infer_from_path(test_image_path)
        
        logger.info(f"Inference completed successfully")
        logger.info(f"Output shape: {output.shape}")
        logger.info(f"Output range: [{output.min():.6f}, {output.max():.6f}]")
        
        # Benchmark
        logger.info("Running benchmark...")
        stats = inferencer.benchmark(test_image_path, num_runs=50)
        
        logger.info("=" * 60)
        logger.info("TENSORRT INFERENCE READY!")
        logger.info("=" * 60)
        logger.info(f"Engine: {engine_path}")
        logger.info(f"Input shape: {inferencer.input_shape}")
        logger.info(f"Performance: {stats['fps']:.2f} FPS ({stats['mean_ms']:.2f} ms)")
        logger.info("=" * 60)
        
        return 0
        
    except Exception as e:
        logger.error(f"Inference failed: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())
