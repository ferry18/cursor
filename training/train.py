#!/usr/bin/env python3
"""
Training script for RF-DETR Nano model with custom dataset
"""

from rfdetr import RFDETRNano
import os
import math

def adjust_dimensions_for_windowed_attention(height: int, width: int, patch_size: int = 16, num_windows: int = 2) -> tuple[int, int]:
    """
    Adjust image dimensions to be compatible with windowed attention.
    
    Args:
        height: Original image height
        width: Original image width
        patch_size: Model patch size (default: 16 for RF-DETR Nano)
        num_windows: Number of windows (default: 2 for RF-DETR Nano)
    
    Returns:
        tuple: (adjusted_height, adjusted_width)
    """
    required_multiple = patch_size * num_windows
    
    # Round up to nearest multiple
    adjusted_height = math.ceil(height / required_multiple) * required_multiple
    adjusted_width = math.ceil(width / required_multiple) * required_multiple
    
    return adjusted_height, adjusted_width

# Define paths
DATASET_PATH = "rfdetr_dataset/"
OUTPUT_PATH = "output/"

# Define the desired resolution as tuple (height, width)
# For RF-DETR Nano: patch_size=16, num_windows=2
# Dimensions must be multiples of patch_size * num_windows = 16 * 2 = 32
ORIGINAL_RESOLUTION = (2080, 3104)  # (height, width) as per your request

# Adjust dimensions to be compatible with windowed attention
RESOLUTION_TUPLE = adjust_dimensions_for_windowed_attention(
    ORIGINAL_RESOLUTION[0], 
    ORIGINAL_RESOLUTION[1]
)

# Create output directory if it doesn't exist
os.makedirs(OUTPUT_PATH, exist_ok=True)

# Initialize the model
model = RFDETRNano()

# Validate dimensions for windowed attention
height, width = RESOLUTION_TUPLE
patch_size = 16  # RF-DETR Nano patch size
num_windows = 2  # RF-DETR Nano number of windows
h_patches = height // patch_size
w_patches = width // patch_size

print(f"Original dimensions: {ORIGINAL_RESOLUTION[1]} × {ORIGINAL_RESOLUTION[0]}")
print(f"Adjusted dimensions: {width} × {height}")
print(f"Patch dimensions: {w_patches} × {h_patches} patches")
print(f"Patches per window: {w_patches // num_windows} × {h_patches // num_windows}")
print(f"Compatible with windowed attention: {h_patches % num_windows == 0 and w_patches % num_windows == 0}")

if h_patches % num_windows != 0 or w_patches % num_windows != 0:
    print(f"ERROR: Dimensions are still not compatible with windowed attention!")
    print(f"Height patches ({h_patches}) must be divisible by num_windows ({num_windows})")
    print(f"Width patches ({w_patches}) must be divisible by num_windows ({num_windows})")
    raise ValueError(f"Dimensions must be multiples of {patch_size * num_windows} = {patch_size * num_windows}")

# Train the model with SPEED-optimized parameters for high-resolution training
# Since you have 64GB RAM and only use 30GB, we can optimize for speed
model.train(
    dataset_dir=DATASET_PATH,
    epochs=20,
    batch_size=2,  # Increased batch size for better GPU utilization
    grad_accum_steps=8,  # Reduced to maintain effective batch size of 16
    lr=1e-4,
    output_dir=OUTPUT_PATH,
    resolution=RESOLUTION_TUPLE[1],  # Using the adjusted width dimension
    # Key parameters for handling your tuple resolution (2080, 3104)
    square_resize_div_64=False,  # This preserves aspect ratio instead of forcing square
    multi_scale=False,  # Enabled for better training and speed
    expanded_scales=False,
    # Training parameters - OPTIMIZED FOR SPEED
    num_workers=2,  # Increased for parallel data loading
    tensorboard=True,
    pin_memory=True,
    early_stopping=True,
    early_stopping_patience=5,
    # Memory optimization - BALANCED FOR SPEED
    amp=True,  # Enable automatic mixed precision
    max_norm=0.1,  # Gradient clipping
    weight_decay=1e-4,
    # Additional optimizations - SPEED FOCUSED
    do_random_resize_via_padding=False,  # Disabled for speed
    warmup_epochs=2,  # Add warmup for stability
    # Dataset fraction for faster training/testing
    dataset_fraction=0.2  # Use only 10% of the dataset
)

print(f"Training completed! Checkpoints and logs saved to {OUTPUT_PATH}")
print(f"Original resolution: {ORIGINAL_RESOLUTION[1]} × {ORIGINAL_RESOLUTION[0]}")
print(f"Adjusted resolution: {RESOLUTION_TUPLE[1]} × {RESOLUTION_TUPLE[0]}")
print(f"Aspect ratio preservation enabled via square_resize_div_64=False")
print(f"Optimized for SPEED with batch_size=2, grad_accum_steps=8, num_workers=4")
print(f"Using 10% of dataset for faster training/testing")
