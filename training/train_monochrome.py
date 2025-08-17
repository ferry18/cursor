#!/usr/bin/env python3
"""
Training script for RF-DETR with monochrome (single-channel) images.
This script uses the monochrome-aware dataset and transforms.
"""

import os
from rfdetr import RFDETRNano

def adjust_dimensions_for_windowed_attention(height: int, width: int, patch_size: int = 16, num_windows: int = 2) -> tuple[int, int]:
    """Adjust dimensions to be compatible with windowed attention."""
    # Calculate how many patches we need
    h_patches = height // patch_size
    w_patches = width // patch_size
    
    # Adjust to ensure patches are divisible by num_windows
    adjusted_h_patches = (h_patches // num_windows) * num_windows
    adjusted_w_patches = (w_patches // num_windows) * num_windows
    
    # Convert back to pixel dimensions
    adjusted_height = adjusted_h_patches * patch_size
    adjusted_width = adjusted_w_patches * patch_size
    
    return adjusted_height, adjusted_width

# Define paths
DATASET_PATH = "rfdetr_dataset/"
OUTPUT_PATH = "output_monochrome/"

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

print(f"\n🚀 Starting MONOCHROME training with RF-DETR Nano")
print(f"📁 Dataset: {DATASET_PATH}")
print(f"📁 Output: {OUTPUT_PATH}")
print(f"🖼️  Resolution: {width} × {height}")
print(f"🎯 Single-channel (monochrome) images will be preserved")

# Train the model with MONOCHROME-optimized parameters
model.train(
    dataset_dir=DATASET_PATH,
    dataset_file='monochrome_roboflow',  # Use monochrome dataset
    epochs=20,
    batch_size=1,  # Conservative batch size for monochrome
    grad_accum_steps=8,  # Maintain effective batch size of 8
    lr=1e-4,
    output_dir=OUTPUT_PATH,
    resolution=RESOLUTION_TUPLE[1],  # Using the adjusted width dimension
    
    # Key parameters for monochrome images
    square_resize_div_64=False,  # Preserve aspect ratio
    multi_scale=False,  # Disabled for stability with monochrome
    expanded_scales=False,
    
    # Training parameters - OPTIMIZED FOR MONOCHROME
    num_workers=4,  # Parallel data loading
    tensorboard=True,
    pin_memory=True,
    early_stopping=True,
    early_stopping_patience=5,
    
    # Memory optimization - BALANCED FOR MONOCHROME
    amp=True,  # Enable automatic mixed precision
    max_norm=0.1,  # Gradient clipping
    weight_decay=1e-4,
    
    # Additional optimizations - MONOCHROME FOCUSED
    do_random_resize_via_padding=False,  # Disabled for speed
    warmup_epochs=2,  # Add warmup for stability
    
    # Dataset fraction for faster training/testing
    dataset_fraction=0.1  # Use only 10% of the dataset for testing
)

print(f"\n✅ Monochrome training completed!")
print(f"📁 Checkpoints and logs saved to {OUTPUT_PATH}")
print(f"🖼️  Original resolution: {ORIGINAL_RESOLUTION[1]} × {ORIGINAL_RESOLUTION[0]}")
print(f"🖼️  Adjusted resolution: {RESOLUTION_TUPLE[1]} × {RESOLUTION_TUPLE[0]}")
print(f"🎯 Single-channel images preserved throughout training")
print(f"⚡ Optimized for monochrome with batch_size=1, grad_accum_steps=8, num_workers=4")
print(f"📊 Using 10% of dataset for faster training/testing")

print(f"\n🔍 Key differences from RGB training:")
print(f"   • Uses 'monochrome_roboflow' dataset file")
print(f"   • Preserves single-channel images (no RGB conversion)")
print(f"   • Uses grayscale normalization [0.5, 0.5] instead of RGB [0.485, 0.456, 0.406]")
print(f"   • Optimized batch size and parameters for single-channel data")
print(f"   • Compatible with your monochrome UAV/Bird detection dataset")
