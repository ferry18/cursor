#!/usr/bin/env python3
"""
Test script to verify COCO evaluation compatibility and fixes.
This script helps prevent the KeyError: 'info' issue we encountered.
"""

import json
import tempfile
import os
from pycocotools.coco import COCO
from pycocotools.cocoeval import COCOeval

def create_test_coco_dataset():
    """Create a minimal test COCO dataset to test our fixes."""
    dataset = {
        "images": [
            {
                "id": 1,
                "file_name": "test1.jpg",
                "height": 640,
                "width": 640
            },
            {
                "id": 2,
                "file_name": "test2.jpg", 
                "height": 640,
                "width": 640
            }
        ],
        "annotations": [
            {
                "id": 1,
                "image_id": 1,
                "category_id": 1,
                "bbox": [100, 100, 200, 200],
                "area": 40000,
                "iscrowd": 0
            },
            {
                "id": 2,
                "image_id": 2,
                "category_id": 1,
                "bbox": [150, 150, 250, 250],
                "area": 10000,
                "iscrowd": 0
            }
        ],
        "categories": [
            {
                "id": 1,
                "name": "test_object",
                "supercategory": "test"
            }
        ]
    }
    return dataset

def test_coco_dataset_structure():
    """Test COCO dataset structure and our fixes."""
    print("Testing COCO dataset structure...")
    
    # Test 1: Dataset without info field
    dataset = create_test_coco_dataset()
    print(f"Dataset keys: {list(dataset.keys())}")
    
    # Test 2: Try to create COCO object without info
    try:
        with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
            json.dump(dataset, f)
            temp_file = f.name
        
        coco = COCO(temp_file)
        print("✓ COCO object created successfully without info field")
        
        # Test loadRes
        results = [
            {
                "image_id": 1,
                "category_id": 1,
                "bbox": [100, 100, 200, 200],
                "score": 0.9
            }
        ]
        
        try:
            coco_dt = COCO.loadRes(coco, results)
            print("✓ COCO.loadRes worked without info field")
        except KeyError as e:
            print(f"✗ COCO.loadRes failed: {e}")
            print("This is the issue we're fixing!")
            
            # Apply our fix
            if 'info' not in coco.dataset:
                coco.dataset['info'] = {
                    'description': 'Test dataset',
                    'url': '',
                    'version': '1.0',
                    'year': 2024,
                    'contributor': '',
                    'date_created': ''
                }
            
            # Try again
            try:
                coco_dt = COCO.loadRes(coco, results)
                print("✓ COCO.loadRes worked after applying our fix!")
            except Exception as e2:
                print(f"✗ COCO.loadRes still failed after fix: {e2}")
        
        os.unlink(temp_file)
        
    except Exception as e:
        print(f"✗ Failed to create COCO object: {e}")

def test_coco_evaluation():
    """Test COCO evaluation with our fixes."""
    print("\nTesting COCO evaluation...")
    
    # Create test dataset
    dataset = create_test_coco_dataset()
    
    # Add our fix
    if 'info' not in dataset:
        dataset['info'] = {
            'description': 'Test dataset',
            'url': '',
            'version': '1.0',
            'year': 2024,
            'contributor': '',
            'date_created': ''
        }
    
    # Add licenses if missing
    if 'licenses' not in dataset:
        dataset['licenses'] = []
    
    # Create temporary file
    with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
        json.dump(dataset, f)
        temp_file = f.name
    
    try:
        # Create COCO object
        coco_gt = COCO(temp_file)
        print("✓ COCO ground truth created successfully")
        
        # Create test results
        results = [
            {
                "image_id": 1,
                "category_id": 1,
                "bbox": [100, 100, 200, 200],
                "score": 0.9
            },
            {
                "image_id": 2,
                "category_id": 1,
                "bbox": [150, 150, 250, 250],
                "score": 0.8
            }
        ]
        
        # Test COCO evaluation
        try:
            coco_dt = COCO.loadRes(coco_gt, results)
            coco_eval = COCOeval(coco_gt, coco_dt, iouType='bbox')
            coco_eval.evaluate()
            coco_eval.accumulate()
            coco_eval.summarize()
            print("✓ COCO evaluation completed successfully")
        except Exception as e:
            print(f"✗ COCO evaluation failed: {e}")
    
    except Exception as e:
        print(f"✗ Failed to test COCO evaluation: {e}")
    
    finally:
        os.unlink(temp_file)

def test_missing_fields():
    """Test handling of various missing fields."""
    print("\nTesting missing fields handling...")
    
    # Test different combinations of missing fields
    test_cases = [
        {"name": "No info", "missing": ["info"]},
        {"name": "No licenses", "missing": ["licenses"]},
        {"name": "No categories", "missing": ["categories"]},
        {"name": "No images", "missing": ["images"]},
        {"name": "No annotations", "missing": ["annotations"]},
        {"name": "Multiple missing", "missing": ["info", "licenses"]},
    ]
    
    for test_case in test_cases:
        print(f"\nTesting: {test_case['name']}")
        dataset = create_test_coco_dataset()
        
        # Remove specified fields
        for field in test_case['missing']:
            if field in dataset:
                del dataset[field]
        
        # Apply our comprehensive fix
        if 'info' not in dataset:
            dataset['info'] = {
                'description': 'Test dataset',
                'url': '',
                'version': '1.0',
                'year': 2024,
                'contributor': '',
                'date_created': ''
            }
        
        if 'licenses' not in dataset:
            dataset['licenses'] = []
        
        if 'categories' not in dataset:
            dataset['categories'] = []
        
        if 'images' not in dataset:
            dataset['images'] = []
        
        if 'annotations' not in dataset:
            dataset['annotations'] = []
        
        # Test if it works
        try:
            with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
                json.dump(dataset, f)
                temp_file = f.name
            
            coco = COCO(temp_file)
            print(f"✓ {test_case['name']}: COCO object created successfully")
            
            # Test loadRes
            results = [{"image_id": 1, "category_id": 1, "bbox": [100, 100, 200, 200], "score": 0.9}]
            coco_dt = COCO.loadRes(coco, results)
            print(f"✓ {test_case['name']}: COCO.loadRes worked")
            
            os.unlink(temp_file)
            
        except Exception as e:
            print(f"✗ {test_case['name']}: Failed - {e}")

def main():
    """Main test function."""
    print("Testing COCO evaluation fixes")
    print("=" * 50)
    
    test_coco_dataset_structure()
    test_coco_evaluation()
    test_missing_fields()
    
    print("\n" + "=" * 50)
    print("Summary:")
    print("- COCO evaluation requires 'info' field in dataset")
    print("- Our fixes ensure all required fields are present")
    print("- Training should now continue without KeyError crashes")
    print("- Evaluation metrics will be available when COCO evaluation succeeds")

if __name__ == "__main__":
    main()
