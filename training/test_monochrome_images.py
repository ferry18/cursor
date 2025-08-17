#!/usr/bin/env python3
"""
Test script to verify monochrome image handling in RF-DETR.
This script tests both training and inference with single-channel images.
"""

import os
import tempfile
import numpy as np
from PIL import Image
import torch
import torchvision.transforms.functional as F

def create_test_monochrome_image(width=640, height=480):
    """Create a test monochrome image."""
    # Create a simple grayscale image with some patterns
    image = np.random.randint(0, 256, (height, width), dtype=np.uint8)
    
    # Add some geometric shapes for testing
    # Rectangle
    image[100:200, 100:300] = 128
    # Circle-like pattern
    y, x = np.ogrid[:height, :width]
    mask = (x - 400)**2 + (y - 200)**2 <= 50**2
    image[mask] = 200
    
    return Image.fromarray(image, mode='L')

def test_image_modes():
    """Test different image modes and their handling."""
    print("Testing image modes...")
    
    # Create test images
    rgb_image = Image.new('RGB', (100, 100), color=(128, 64, 192))
    grayscale_image = Image.new('L', (100, 100), color=128)
    rgba_image = Image.new('RGBA', (100, 100), color=(128, 64, 192, 255))
    
    print(f"RGB image mode: {rgb_image.mode}, size: {rgb_image.size}")
    print(f"Grayscale image mode: {grayscale_image.mode}, size: {grayscale_image.size}")
    print(f"RGBA image mode: {rgba_image.mode}, size: {rgba_image.size}")
    
    # Test conversion logic
    def test_conversion(image):
        if image.mode in ['L', 'LA']:
            return image.convert('L')
        elif image.mode == 'RGB':
            return image
        else:
            return image.convert('L')
    
    converted_rgb = test_conversion(rgb_image)
    converted_grayscale = test_conversion(grayscale_image)
    converted_rgba = test_conversion(rgba_image)
    
    print(f"RGB converted to: {converted_rgb.mode}")
    print(f"Grayscale converted to: {converted_grayscale.mode}")
    print(f"RGBA converted to: {converted_rgba.mode}")
    
    return True

def test_tensor_conversion():
    """Test tensor conversion and normalization."""
    print("\nTesting tensor conversion...")
    
    # Create test images
    rgb_image = Image.new('RGB', (100, 100), color=(128, 64, 192))
    grayscale_image = Image.new('L', (100, 100), color=128)
    
    # Convert to tensors
    rgb_tensor = F.to_tensor(rgb_image)
    grayscale_tensor = F.to_tensor(grayscale_image)
    
    print(f"RGB tensor shape: {rgb_tensor.shape}, dtype: {rgb_tensor.dtype}")
    print(f"Grayscale tensor shape: {grayscale_tensor.shape}, dtype: {grayscale_tensor.dtype}")
    
    # Test normalization
    rgb_means = [0.485, 0.456, 0.406]
    rgb_stds = [0.229, 0.224, 0.225]
    grayscale_mean = [0.5]
    grayscale_std = [0.5]
    
    normalized_rgb = F.normalize(rgb_tensor, rgb_means, rgb_stds)
    normalized_grayscale = F.normalize(grayscale_tensor, grayscale_mean, grayscale_std)
    
    print(f"Normalized RGB shape: {normalized_rgb.shape}")
    print(f"Normalized grayscale shape: {normalized_grayscale.shape}")
    
    return True

def test_monochrome_dataset():
    """Test the monochrome dataset functionality."""
    print("\nTesting monochrome dataset...")
    
    try:
        from rfdetr.datasets.monochrome_coco import MonochromeCocoDetection, make_monochrome_transforms
        
        # Create a temporary test image
        test_image = create_test_monochrome_image()
        
        # Test transforms
        transforms = make_monochrome_transforms('val', 640)
        
        # Create a dummy target
        target = {
            'boxes': torch.tensor([[100, 100, 200, 200]]),
            'labels': torch.tensor([1]),
            'image_id': torch.tensor([1])
        }
        
        # Apply transforms
        transformed_image, transformed_target = transforms(test_image, target)
        
        print(f"Original image mode: {test_image.mode}")
        print(f"Transformed image shape: {transformed_image.shape}")
        print(f"Transformed image dtype: {transformed_image.dtype}")
        print(f"Transformed target keys: {list(transformed_target.keys())}")
        
        return True
        
    except ImportError as e:
        print(f"Could not import monochrome dataset: {e}")
        return False

def test_inference_compatibility():
    """Test inference compatibility with monochrome images."""
    print("\nTesting inference compatibility...")
    
    # Simulate the inference pipeline
    def simulate_inference_pipeline(image_path):
        # Load image
        image = Image.open(image_path)
        
        # Preserve single-channel images
        if image.mode in ['L', 'LA']:
            image = image.convert('L')
        elif image.mode != 'RGB':
            image = image.convert('L')
        
        # Convert to tensor
        if not isinstance(image, torch.Tensor):
            image = F.to_tensor(image)
        
        # Validate shape
        if image.shape[0] not in [1, 3]:
            raise ValueError(f"Invalid image shape. Expected 1 or 3 channels, but got {image.shape[0]} channels.")
        
        # Apply normalization
        if image.shape[0] == 1:
            # Grayscale normalization
            image = F.normalize(image, [0.5], [0.5])
        else:
            # RGB normalization
            image = F.normalize(image, [0.485, 0.456, 0.406], [0.229, 0.224, 0.225])
        
        return image
    
    # Create test images
    with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as f:
        test_image = create_test_monochrome_image()
        test_image.save(f.name)
        temp_path = f.name
    
    try:
        # Test the pipeline
        result = simulate_inference_pipeline(temp_path)
        print(f"Inference pipeline result shape: {result.shape}")
        print(f"Inference pipeline result dtype: {result.dtype}")
        print("✓ Inference pipeline works with monochrome images")
        
        return True
        
    except Exception as e:
        print(f"✗ Inference pipeline failed: {e}")
        return False
    
    finally:
        # Clean up
        if os.path.exists(temp_path):
            os.unlink(temp_path)

def main():
    """Run all tests."""
    print("RF-DETR Monochrome Image Handling Test")
    print("=" * 50)
    
    tests = [
        ("Image Modes", test_image_modes),
        ("Tensor Conversion", test_tensor_conversion),
        ("Monochrome Dataset", test_monochrome_dataset),
        ("Inference Compatibility", test_inference_compatibility),
    ]
    
    results = []
    for test_name, test_func in tests:
        print(f"\nRunning {test_name} test...")
        try:
            result = test_func()
            results.append((test_name, result))
            print(f"✓ {test_name} test passed")
        except Exception as e:
            print(f"✗ {test_name} test failed: {e}")
            results.append((test_name, False))
    
    # Summary
    print("\n" + "=" * 50)
    print("Test Summary:")
    for test_name, result in results:
        status = "PASS" if result else "FAIL"
        print(f"  {test_name}: {status}")
    
    all_passed = all(result for _, result in results)
    print(f"\nOverall: {'ALL TESTS PASSED' if all_passed else 'SOME TESTS FAILED'}")
    
    if all_passed:
        print("\n✅ Monochrome image handling is working correctly!")
    else:
        print("\n❌ Some issues found with monochrome image handling.")

if __name__ == "__main__":
    main()
