# RF-DETR Windowed Attention Compatibility Guide

## Problem Description

The RF-DETR model uses a windowed attention mechanism that divides the image into windows for efficient processing. However, this mechanism has specific requirements for image dimensions that can cause runtime errors when using non-square images.

### The Error
```
RuntimeError: shape '[1, 2, 89, 2, 89, -1]' is invalid for input of size 13260288
```

This error occurs in the windowed attention mechanism when trying to reshape tensors with incompatible dimensions.

## Root Cause Analysis

### 1. Windowed Attention Requirements
- **Patch Size**: Each model variant has a specific patch size (16 for Nano/Small/Medium, 14 for Base/Large)
- **Number of Windows**: Each model variant divides the image into a specific number of windows (2 for Nano/Small/Medium, 4 for Base/Large)
- **Dimension Constraint**: Image dimensions must be multiples of `patch_size × num_windows`

### 2. Model Variants and Requirements

| Model | Patch Size | Num Windows | Required Multiple |
|-------|------------|-------------|-------------------|
| Nano  | 16         | 2           | 32                |
| Small | 16         | 2           | 32                |
| Medium| 16         | 2           | 32                |
| Base  | 14         | 4           | 56                |
| Large | 14         | 4           | 56                |

### 3. The Bug in the Code
The original code had a bug in the windowing logic:

```python
# BUGGY CODE (line 315 in dinov2_with_windowed_attn.py)
windowed_pixel_tokens = pixel_tokens_with_pos_embed.view(
    batch_size, num_windows, num_h_patches_per_window, 
    num_windows, num_h_patches_per_window, -1  # ❌ Wrong dimension!
)
```

Should be:
```python
# FIXED CODE
windowed_pixel_tokens = pixel_tokens_with_pos_embed.view(
    batch_size, num_windows, num_h_patches_per_window, 
    num_windows, num_w_patches_per_window, -1  # ✅ Correct dimension!
)
```

## Solution Implementation

### 1. Fixed the Core Bug
- **File**: `rfdetr/models/backbone/dinov2_with_windowed_attn.py`
- **Line 315**: Fixed the tensor reshaping to use correct dimensions
- **Line 627**: Fixed the reverse transformation in attention layer

### 2. Added Validation
- **File**: `rfdetr/models/backbone/dinov2_with_windowed_attn.py`
- Added dimension validation before windowing to catch issues early
- **File**: `rfdetr/config.py`
- Added `validate_dimensions()` method to ModelConfig class

### 3. Enhanced Training Script
- **File**: `train.py`
- Added automatic dimension adjustment
- Added validation and debugging output
- Added helper function for dimension compatibility

## How to Use

### 1. Automatic Dimension Adjustment
```python
from train import adjust_dimensions_for_windowed_attention

# Original dimensions
original_height, original_width = 2080, 3104

# Adjust for RF-DETR Nano
adjusted_height, adjusted_width = adjust_dimensions_for_windowed_attention(
    original_height, original_width, 
    patch_size=16, num_windows=2
)

print(f"Original: {original_width}×{original_height}")
print(f"Adjusted: {adjusted_width}×{adjusted_height}")
```

### 2. Manual Validation
```python
from rfdetr.config import RFDETRNanoConfig

config = RFDETRNanoConfig()
config.validate_dimensions(height=2080, width=3104)
```

### 3. Testing Compatibility
```bash
python test_windowed_attention.py
```

## Best Practices

### 1. Always Validate Dimensions
Before training, ensure your image dimensions are compatible:

```python
def is_compatible(height, width, patch_size, num_windows):
    required_multiple = patch_size * num_windows
    return (height % required_multiple == 0 and 
            width % required_multiple == 0)
```

### 2. Use Appropriate Resolutions
Common compatible resolutions for different models:

**RF-DETR Nano/Small/Medium (32× multiplier):**
- 640×640, 1024×1024, 1920×1080, 2048×1024
- 3104×2080 (your case: 3104×2080 is compatible!)

**RF-DETR Base/Large (56× multiplier):**
- 560×560, 1120×1120, 1792×1008, 2016×1008

### 3. Preserve Aspect Ratio
Use `square_resize_div_64=False` to maintain aspect ratio:

```python
model.train(
    dataset_dir="your_dataset",
    resolution=3104,  # Use the larger dimension
    square_resize_div_64=False,  # Preserve aspect ratio
    # ... other parameters
)
```

## Prevention Strategies

### 1. Add Dimension Checks
Always validate dimensions before model initialization:

```python
def validate_model_dimensions(model_class, height, width):
    if model_class == RFDETRNano:
        patch_size, num_windows = 16, 2
    # ... other models
    
    required_multiple = patch_size * num_windows
    if height % required_multiple != 0 or width % required_multiple != 0:
        raise ValueError(f"Dimensions must be multiples of {required_multiple}")
```

### 2. Use Type Hints and Validation
```python
from pydantic import BaseModel, validator

class TrainingConfig(BaseModel):
    height: int
    width: int
    model_type: str
    
    @validator('height', 'width')
    def validate_dimensions(cls, v, values):
        model_type = values.get('model_type')
        if model_type == 'nano':
            required_multiple = 32
        elif model_type == 'base':
            required_multiple = 56
        else:
            return v
            
        if v % required_multiple != 0:
            raise ValueError(f"Dimension must be multiple of {required_multiple}")
        return v
```

### 3. Add Unit Tests
```python
def test_windowed_attention_compatibility():
    test_cases = [
        (640, 640, True),   # Compatible
        (1920, 1080, True), # Compatible
        (1000, 1000, False), # Incompatible
    ]
    
    for height, width, expected in test_cases:
        result = is_compatible(height, width, 16, 2)
        assert result == expected
```

## Common Pitfalls

### 1. Assuming Square Images Only
❌ **Wrong**: "RF-DETR only works with square images"
✅ **Correct**: RF-DETR works with any dimensions that are multiples of `patch_size × num_windows`

### 2. Ignoring Model Variants
❌ **Wrong**: Using the same resolution for all model variants
✅ **Correct**: Check the specific requirements for your model variant

### 3. Not Testing Edge Cases
❌ **Wrong**: Only testing with standard resolutions
✅ **Correct**: Test with your actual use case dimensions

## Debugging Tips

### 1. Check Patch Dimensions
```python
patch_size = 16  # or 14 for base/large
num_windows = 2  # or 4 for base/large
h_patches = height // patch_size
w_patches = width // patch_size
print(f"Patches: {w_patches}×{h_patches}")
print(f"Per window: {w_patches//num_windows}×{h_patches//num_windows}")
```

### 2. Enable Debug Output
```python
import logging
logging.basicConfig(level=logging.DEBUG)
```

### 3. Use the Test Script
```bash
python test_windowed_attention.py
```

## Conclusion

The windowed attention mechanism in RF-DETR is powerful but requires careful attention to dimension constraints. By following this guide and using the provided tools, you can:

1. ✅ Train on non-square images
2. ✅ Preserve aspect ratios
3. ✅ Avoid runtime errors
4. ✅ Optimize for your specific use case

The key is understanding that **RF-DETR supports non-square images** as long as the dimensions are compatible with the windowing mechanism.
