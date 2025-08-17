# ------------------------------------------------------------------------
# RF-DETR
# Copyright (c) 2025 Roboflow. All Rights Reserved.
# Licensed under the Apache License, Version 2.0 [see LICENSE for details]
# ------------------------------------------------------------------------
# Modified from LW-DETR (https://github.com/Atten4Vis/LW-DETR)
# Copyright (c) 2024 Baidu. All Rights Reserved.
# ------------------------------------------------------------------------
# Modified from Conditional DETR (https://github.com/Atten4Vis/ConditionalDETR)
# Copyright (c) 2021 Microsoft. All Rights Reserved.
# ------------------------------------------------------------------------
# Copied from DETR (https://github.com/facebookresearch/detr)
# Copyright (c) Facebook, Inc. and its affiliates. All Rights Reserved.
# ------------------------------------------------------------------------

"""
Monochrome-aware COCO dataset which preserves single-channel images.
"""

import os
from pathlib import Path

import torch
import torch.utils.data
import torchvision

import rfdetr.datasets.transforms as T


class MonochromeCocoDetection(torchvision.datasets.CocoDetection):
    """
    COCO dataset that preserves single-channel (monochrome) images.
    Unlike the standard CocoDetection, this class does not force RGB conversion.
    """
    
    def _load_image(self, id: int):
        path = self.coco.loadImgs(id)[0]["file_name"]
        image = torchvision.datasets.folder.default_loader(os.path.join(self.root, path))
        
        # Preserve single-channel images instead of forcing RGB conversion
        if image.mode in ['L', 'LA']:
            # Keep as grayscale (single channel)
            return image.convert('L')
        elif image.mode == 'RGB':
            # Keep as RGB
            return image
        else:
            # Convert other formats to grayscale
            return image.convert('L')


def make_monochrome_transforms(image_set, resolution, multi_scale=False, expanded_scales=False, skip_random_resize=False, patch_size=16, num_windows=4):
    """
    Create transforms for monochrome images with appropriate normalization.
    """
    
    # Use grayscale normalization for single-channel images
    normalize = T.Compose([
        T.ToTensor(),
        T.Normalize([0.5], [0.5])  # Grayscale normalization: mean=0.5, std=0.5
    ])

    scales = [resolution]
    if multi_scale:
        # scales = [448, 512, 576, 640, 704, 768, 832, 896]
        scales = compute_multi_scale_scales(resolution, expanded_scales, patch_size, num_windows)
        if skip_random_resize:
            scales = [scales[-1]]
        print(scales)

    if image_set == 'train':
        return T.Compose([
            T.RandomHorizontalFlip(),
            T.RandomSelect(
                T.RandomResize(scales, max_size=resolution),
                T.Compose([
                    T.RandomResize([400, 500, 600]),
                    T.RandomSizeCrop(384, 600),
                    T.RandomResize(scales, max_size=resolution),
                ])
            ),
            normalize,
        ])

    if image_set == 'val':
        return T.Compose([
            T.RandomResize([resolution], max_size=resolution),
            normalize,
        ])
    if image_set == 'test':
        return T.Compose([
            T.RandomResize([resolution], max_size=resolution),
            normalize,
        ])
    if image_set == 'val_speed':
        return T.Compose([
            T.SquareResize([resolution]),
            normalize,
        ])

    raise ValueError(f'unknown {image_set}')


def make_monochrome_transforms_square_div_64(image_set, resolution, multi_scale=False, expanded_scales=False, skip_random_resize=False, patch_size=16, num_windows=4):
    """
    Create square resize transforms for monochrome images.
    """
    
    # Use grayscale normalization for single-channel images
    normalize = T.Compose([
        T.ToTensor(),
        T.Normalize([0.5], [0.5])  # Grayscale normalization: mean=0.5, std=0.5
    ])

    scales = [resolution]
    if multi_scale:
        # scales = [448, 512, 576, 640, 704, 768, 832, 896]
        scales = compute_multi_scale_scales(resolution, expanded_scales, patch_size, num_windows)
        if skip_random_resize:
            scales = [scales[-1]]
        print(scales)

    if image_set == 'train':
        return T.Compose([
            T.RandomHorizontalFlip(),
            T.RandomSelect(
                T.SquareResize(scales),
                T.Compose([
                    T.RandomResize([400, 500, 600]),
                    T.RandomSizeCrop(384, 600),
                    T.SquareResize(scales),
                ]),
            ),
            normalize,
        ])

    if image_set == 'val':
        return T.Compose([
            T.SquareResize([resolution]),
            normalize,
        ])
    if image_set == 'test':
        return T.Compose([
            T.SquareResize([resolution]),
            normalize,
        ])
    if image_set == 'val_speed':
        return T.Compose([
            T.SquareResize([resolution]),
            normalize,
        ])

    raise ValueError(f'unknown {image_set}')


def compute_multi_scale_scales(resolution, expanded_scales=False, patch_size=16, num_windows=4):
    # round to the nearest multiple of 4*patch_size to enable both patching and windowing
    base_num_patches_per_window = resolution // (patch_size * num_windows)
    offsets = [-3, -2, -1, 0, 1, 2, 3, 4] if not expanded_scales else [-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5]
    scales = [base_num_patches_per_window + offset for offset in offsets]
    proposed_scales = [scale * patch_size * num_windows for scale in scales]
    proposed_scales = [scale for scale in proposed_scales if scale >= patch_size * num_windows * 2]  # ensure minimum image size
    return proposed_scales


def build_monochrome(image_set, args, resolution):
    """
    Build monochrome-aware COCO dataset.
    """
    root = Path(args.coco_path)
    assert root.exists(), f'provided COCO path {root} does not exist'
    mode = 'instances'
    PATHS = {
        "train": (root / "train2017", root / "annotations" / f'{mode}_train2017.json'),
        "val": (root /  "val2017", root / "annotations" / f'{mode}_val2017.json'),
        "test": (root / "test2017", root / "annotations" / f'image_info_test-dev2017.json'),
    }
    
    img_folder, ann_file = PATHS[image_set.split("_")[0]]
    
    try:
        square_resize = args.square_resize
    except:
        square_resize = False
    
    try:
        square_resize_div_64 = args.square_resize_div_64
    except:
        square_resize_div_64 = False

    if square_resize_div_64:
        dataset = MonochromeCocoDetection(img_folder, ann_file, transforms=make_monochrome_transforms_square_div_64(
            image_set,
            resolution,
            multi_scale=args.multi_scale,
            expanded_scales=args.expanded_scales,
            skip_random_resize=not args.do_random_resize_via_padding,
            patch_size=args.patch_size,
            num_windows=args.num_windows
        ))
    else:
        dataset = MonochromeCocoDetection(img_folder, ann_file, transforms=make_monochrome_transforms(
            image_set,
            resolution,
            multi_scale=args.multi_scale,
            expanded_scales=args.expanded_scales,
            skip_random_resize=not args.do_random_resize_via_padding,
            patch_size=args.patch_size,
            num_windows=args.num_windows
        ))
    return dataset


def build_monochrome_roboflow(image_set, args, resolution):
    """
    Build monochrome-aware Roboflow dataset.
    """
    root = Path(args.dataset_dir)
    assert root.exists(), f'provided Roboflow path {root} does not exist'
    mode = 'instances'
    PATHS = {
        "train": (root / "train", root / "train" / "_annotations.coco.json"),
        "val": (root /  "valid", root / "valid" / "_annotations.coco.json"),
        "test": (root / "test", root / "test" / "_annotations.coco.json"),
    }
    
    img_folder, ann_file = PATHS[image_set.split("_")[0]]
    
    try:
        square_resize = args.square_resize
    except:
        square_resize = False
    
    try:
        square_resize_div_64 = args.square_resize_div_64
    except:
        square_resize_div_64 = False

    if square_resize_div_64:
        dataset = MonochromeCocoDetection(img_folder, ann_file, transforms=make_monochrome_transforms_square_div_64(
            image_set,
            resolution,
            multi_scale=args.multi_scale,
            expanded_scales=args.expanded_scales,
            skip_random_resize=not args.do_random_resize_via_padding,
            patch_size=args.patch_size,
            num_windows=args.num_windows
        ))
    else:
        dataset = MonochromeCocoDetection(img_folder, ann_file, transforms=make_monochrome_transforms(
            image_set,
            resolution,
            multi_scale=args.multi_scale,
            expanded_scales=args.expanded_scales,
            skip_random_resize=not args.do_random_resize_via_padding,
            patch_size=args.patch_size,
            num_windows=args.num_windows
        ))
    return dataset
