#!/usr/bin/env python3
"""
TensorRT Evaluation Script for RF-DETR Monochrome Model
Runs inference on full resolution images and calculates COCO-style metrics for UAV class
"""

import os
import sys
import json
import random
import numpy as np
import cv2
import tensorrt as trt
import pycuda.driver as cuda
import pycuda.autoinit
import time
from pathlib import Path
from typing import List, Tuple, Dict, Any, Optional
import logging
from collections import defaultdict
# import matplotlib.pyplot as plt
# import matplotlib.patches as patches
from pycocotools.coco import COCO
from pycocotools.cocoeval import COCOeval

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class RFDETRTensorRTEvaluator:
    """
    TensorRT evaluator for RF-DETR model with full resolution inference
    """
    
    def __init__(self, engine_path: str, confidence_threshold: float = 0.5, nms_threshold: float = 0.5):
        """
        Initialize TensorRT evaluator
        
        Args:
            engine_path: Path to TensorRT engine file
            confidence_threshold: Minimum confidence for detections
            nms_threshold: NMS threshold for removing duplicate detections
        """
        self.engine_path = engine_path
        self.confidence_threshold = confidence_threshold
        self.nms_threshold = nms_threshold
        
        # Load TensorRT engine
        self.engine = self._load_engine()
        self.context = self.engine.create_execution_context()
        
        # Get engine specifications
        self.input_name = self.engine.get_tensor_name(0)
        self.dets_name = self.engine.get_tensor_name(1)
        self.labels_name = self.engine.get_tensor_name(2)
        
        self.input_shape = self.engine.get_tensor_shape(self.input_name)
        self.dets_shape = self.engine.get_tensor_shape(self.dets_name)
        self.labels_shape = self.engine.get_tensor_shape(self.labels_name)
        
        logger.info(f"Engine loaded: {engine_path}")
        logger.info(f"Input shape: {self.input_shape}")
        logger.info(f"Dets shape: {self.dets_shape}")
        logger.info(f"Labels shape: {self.labels_shape}")
        
        # Allocate GPU memory
        self._allocate_memory()
        
    def _load_engine(self) -> trt.ICudaEngine:
        """Load TensorRT engine from file"""
        with open(self.engine_path, 'rb') as f:
            engine_data = f.read()
        
        runtime = trt.Runtime(trt.Logger(trt.Logger.WARNING))
        engine = runtime.deserialize_cuda_engine(engine_data)
        return engine
    
    def _allocate_memory(self):
        """Allocate GPU memory for inputs and outputs"""
        # Calculate sizes
        input_size = int(np.prod(self.input_shape) * 4)  # float32
        dets_size = int(np.prod(self.dets_shape) * 4)
        labels_size = int(np.prod(self.labels_shape) * 4)
        
        # Allocate GPU memory
        self.d_input = cuda.mem_alloc(input_size)
        self.d_dets = cuda.mem_alloc(dets_size)
        self.d_labels = cuda.mem_alloc(labels_size)
        
        # Create CUDA stream
        self.stream = cuda.Stream()
        
        # Set tensor addresses
        self.context.set_tensor_address(self.input_name, int(self.d_input))
        self.context.set_tensor_address(self.dets_name, int(self.d_dets))
        self.context.set_tensor_address(self.labels_name, int(self.d_labels))
    
    def preprocess_image(self, image: np.ndarray, target_size: Tuple[int, int] = (384, 384)) -> np.ndarray:
        """
        Preprocess image for TensorRT inference
        
        Args:
            image: Input image (H, W) or (H, W, 3)
            target_size: Target size for engine input
            
        Returns:
            Preprocessed image tensor
        """
        # Ensure image is grayscale
        if len(image.shape) == 3:
            image = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
        
        # Resize to target size
        image_resized = cv2.resize(image, target_size, interpolation=cv2.INTER_LINEAR)
        
        # Convert to 3-channel by repeating
        image_3ch = np.stack([image_resized, image_resized, image_resized], axis=2)
        
        # Normalize to [0, 1]
        image_normalized = image_3ch.astype(np.float32) / 255.0
        
        # Transpose to CHW format and add batch dimension
        image_tensor = np.transpose(image_normalized, (2, 0, 1))
        image_tensor = np.expand_dims(image_tensor, axis=0)
        
        return np.ascontiguousarray(image_tensor)
    
    def infer(self, image: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        """
        Run inference on preprocessed image
        
        Args:
            image: Preprocessed image tensor
            
        Returns:
            Tuple of (detections, labels)
        """
        # Copy input to GPU
        cuda.memcpy_htod_async(self.d_input, image, self.stream)
        
        # Execute inference
        self.context.execute_async_v3(stream_handle=self.stream.handle)
        
        # Prepare output arrays
        dets_output = np.empty(self.dets_shape, dtype=np.float32)
        labels_output = np.empty(self.labels_shape, dtype=np.float32)
        
        # Copy outputs from GPU
        cuda.memcpy_dtoh_async(dets_output, self.d_dets, self.stream)
        cuda.memcpy_dtoh_async(labels_output, self.d_labels, self.stream)
        
        # Synchronize
        self.stream.synchronize()
        
        return dets_output, labels_output
    
    def postprocess_detections(self, dets: np.ndarray, labels: np.ndarray, 
                              original_size: Tuple[int, int]) -> List[Dict[str, Any]]:
        """
        Postprocess detections to get bounding boxes and scores
        
        Args:
            dets: Detection boxes from model (1, 300, 4)
            labels: Classification scores from model (1, 300, 2)
            original_size: Original image size (width, height)
            
        Returns:
            List of detection dictionaries
        """
        detections = []
        
        # Reshape outputs
        dets = dets[0]  # (300, 4)
        labels = labels[0]  # (300, 2)
        
        # Get scores for UAV class (class 0) - convert logits to probabilities
        logits = labels[:, :]  # (300, 2) - UAV and background logits
        scores = np.exp(logits[:, 0]) / (np.exp(logits[:, 0]) + np.exp(logits[:, 1]))  # Softmax for UAV class
        
        # Filter by confidence threshold
        valid_indices = np.where(scores > self.confidence_threshold)[0]
        
        for idx in valid_indices:
            score = scores[idx]
            bbox = dets[idx]  # [x1, y1, x2, y2] in normalized coordinates
            
            # Convert to pixel coordinates
            x1, y1, x2, y2 = bbox
            x1 *= original_size[0]
            y1 *= original_size[1]
            x2 *= original_size[0]
            y2 *= original_size[1]
            
            # Convert to [x, y, width, height] format
            x, y, w, h = x1, y1, x2 - x1, y2 - y1
            
            detection = {
                'bbox': [x, y, w, h],
                'score': float(score),
                'category_id': 0,  # UAV class
                'category_name': 'UAV'
            }
            detections.append(detection)
        
        # Apply NMS
        detections = self._apply_nms(detections)
        
        return detections
    
    def _apply_nms(self, detections: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        """
        Apply Non-Maximum Suppression to remove duplicate detections
        
        Args:
            detections: List of detection dictionaries
            
        Returns:
            Filtered detections after NMS
        """
        if not detections:
            return []
        
        # Convert to numpy arrays
        bboxes = np.array([d['bbox'] for d in detections])
        scores = np.array([d['score'] for d in detections])
        
        # Convert [x, y, w, h] to [x1, y1, x2, y2]
        x1 = bboxes[:, 0]
        y1 = bboxes[:, 1]
        x2 = bboxes[:, 0] + bboxes[:, 2]
        y2 = bboxes[:, 1] + bboxes[:, 3]
        
        # Calculate areas
        areas = (x2 - x1) * (y2 - y1)
        
        # Sort by score
        order = scores.argsort()[::-1]
        
        keep = []
        while order.size > 0:
            i = order[0]
            keep.append(i)
            
            if order.size == 1:
                break
            
            # Calculate IoU with remaining boxes
            xx1 = np.maximum(x1[i], x1[order[1:]])
            yy1 = np.maximum(y1[i], y1[order[1:]])
            xx2 = np.minimum(x2[i], x2[order[1:]])
            yy2 = np.minimum(y2[i], y2[order[1:]])
            
            w = np.maximum(0.0, xx2 - xx1)
            h = np.maximum(0.0, yy2 - yy1)
            inter = w * h
            
            ovr = inter / (areas[i] + areas[order[1:]] - inter)
            
            # Keep boxes with IoU below threshold
            inds = np.where(ovr <= self.nms_threshold)[0]
            order = order[inds + 1]
        
        return [detections[i] for i in keep]
    
    def visualize_detections(self, image: np.ndarray, detections: List[Dict[str, Any]], 
                           output_path: str, ground_truth: List[Dict[str, Any]] = None):
        """
        Visualize detections on image and save to file
        
        Args:
            image: Original image
            detections: List of detection dictionaries
            output_path: Path to save visualization
            ground_truth: Optional ground truth annotations
        """
        # Convert to RGB if grayscale
        if len(image.shape) == 2:
            vis_image = cv2.cvtColor(image, cv2.COLOR_GRAY2BGR)
        else:
            vis_image = image.copy()
        
        # Draw predictions (red)
        for det in detections:
            bbox = det['bbox']
            score = det['score']
            
            x, y, w, h = bbox
            x1, y1, x2, y2 = int(x), int(y), int(x + w), int(y + h)
            
            # Draw bounding box
            cv2.rectangle(vis_image, (x1, y1), (x2, y2), (0, 0, 255), 2)
            
            # Draw label
            label = f"UAV: {score:.3f}"
            cv2.putText(vis_image, label, (x1, y1 - 10), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 2)
        
        # Draw ground truth (green) if provided
        if ground_truth:
            for gt in ground_truth:
                bbox = gt['bbox']
                x, y, w, h = bbox
                x1, y1, x2, y2 = int(x), int(y), int(x + w), int(y + h)
                
                # Draw bounding box
                cv2.rectangle(vis_image, (x1, y1), (x2, y2), (0, 255, 0), 2)
                
                # Draw label
                cv2.putText(vis_image, "GT: UAV", (x1, y1 - 30), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)
        
        # Save visualization
        cv2.imwrite(output_path, vis_image)
        logger.info(f"Visualization saved: {output_path}")

def load_coco_annotations(annotation_file: str) -> Dict[str, Any]:
    """
    Load COCO format annotations
    
    Args:
        annotation_file: Path to COCO annotation file
        
    Returns:
        Dictionary containing annotations
    """
    with open(annotation_file, 'r') as f:
        annotations = json.load(f)
    return annotations

def filter_uav_annotations(annotations: Dict[str, Any]) -> Dict[str, Any]:
    """
    Filter annotations to only include UAV class (id 0)
    
    Args:
        annotations: COCO annotations dictionary
        
    Returns:
        Filtered annotations with only UAV class
    """
    # Filter annotations to only UAV class (id 0)
    filtered_annotations = annotations.copy()
    filtered_annotations['annotations'] = [
        ann for ann in annotations['annotations'] 
        if ann['category_id'] == 0  # UAV class
    ]
    
    # Keep only images that have UAV annotations
    uav_image_ids = set(ann['image_id'] for ann in filtered_annotations['annotations'])
    filtered_annotations['images'] = [
        img for img in annotations['images'] 
        if img['id'] in uav_image_ids
    ]
    
    return filtered_annotations

def calculate_coco_metrics(predictions: List[Dict[str, Any]], 
                          ground_truth: Dict[str, Any]) -> Dict[str, float]:
    """
    Calculate COCO-style metrics for UAV class
    
    Args:
        predictions: List of prediction dictionaries
        ground_truth: Ground truth annotations
        
    Returns:
        Dictionary of metrics
    """
    # Create COCO objects for evaluation
    coco_gt = COCO()
    coco_gt.dataset = ground_truth
    coco_gt.createIndex()
    
    # Convert predictions to COCO format
    coco_predictions = []
    for pred in predictions:
        coco_pred = {
            'image_id': pred['image_id'],
            'category_id': pred['category_id'],
            'bbox': pred['bbox'],
            'score': pred['score']
        }
        coco_predictions.append(coco_pred)
    
    # If no predictions, create a dummy prediction to avoid COCO evaluation error
    if not coco_predictions:
        logger.warning("No predictions found, creating dummy prediction for COCO evaluation")
        coco_predictions = [{
            'image_id': 0,
            'category_id': 0,
            'bbox': [0, 0, 1, 1],
            'score': 0.0
        }]
    
    # Create COCO eval object
    coco_eval = COCOeval(coco_gt, iouType='bbox')
    coco_eval.cocoDt = coco_gt.loadRes(coco_predictions)
    coco_eval.evaluate()
    coco_eval.accumulate()
    coco_eval.summarize()
    
    # Extract metrics
    metrics = {
        'AP': coco_eval.stats[0],  # AP @[ IoU=0.50:0.95 | area=   all | maxDets=100 ]
        'AP50': coco_eval.stats[1],  # AP @[ IoU=0.50      | area=   all | maxDets=100 ]
        'AP75': coco_eval.stats[2],  # AP @[ IoU=0.75      | area=   all | maxDets=100 ]
        'APs': coco_eval.stats[3],  # AP @[ IoU=0.50:0.95 | area= small | maxDets=100 ]
        'APm': coco_eval.stats[4],  # AP @[ IoU=0.50:0.95 | area=medium | maxDets=100 ]
        'APl': coco_eval.stats[5],  # AP @[ IoU=0.50:0.95 | area= large | maxDets=100 ]
        'AR1': coco_eval.stats[6],  # AR @[ IoU=0.50:0.95 | area=   all | maxDets=  1 ]
        'AR10': coco_eval.stats[7],  # AR @[ IoU=0.50:0.95 | area=   all | maxDets= 10 ]
        'AR100': coco_eval.stats[8],  # AR @[ IoU=0.50:0.95 | area=   all | maxDets=100 ]
        'ARs': coco_eval.stats[9],  # AR @[ IoU=0.50:0.95 | area= small | maxDets=100 ]
        'ARm': coco_eval.stats[10],  # AR @[ IoU=0.50:0.95 | area=medium | maxDets=100 ]
        'ARl': coco_eval.stats[11],  # AR @[ IoU=0.50:0.95 | area= large | maxDets=100 ]
    }
    
    return metrics

def main():
    """Main evaluation function"""
    # Configuration
    engine_path = "rfdetr_monochrome_int8.engine"
    annotation_file = "rfdetr_dataset/test/_annotations.coco.json"
    image_dir = "rfdetr_dataset/test"
    output_dir = "evaluation_results"
    sample_percentage = 0.1  # 10% of images
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    os.makedirs(os.path.join(output_dir, "visualizations"), exist_ok=True)
    
    # Load annotations
    logger.info("Loading annotations...")
    annotations = load_coco_annotations(annotation_file)
    uav_annotations = filter_uav_annotations(annotations)
    
    logger.info(f"Total images: {len(annotations['images'])}")
    logger.info(f"UAV images: {len(uav_annotations['images'])}")
    logger.info(f"UAV annotations: {len(uav_annotations['annotations'])}")
    
    # Sample 10% of UAV images
    uav_images = uav_annotations['images']
    num_samples = max(1, int(len(uav_images) * sample_percentage))
    sampled_images = random.sample(uav_images, num_samples)
    
    logger.info(f"Sampling {num_samples} images ({sample_percentage*100}%)")
    
    # Initialize TensorRT evaluator
    logger.info("Initializing TensorRT evaluator...")
    evaluator = RFDETRTensorRTEvaluator(engine_path, confidence_threshold=0.5, nms_threshold=0.5)
    
    # Run evaluation
    all_predictions = []
    all_ground_truth = []
    
    logger.info("Running inference...")
    for i, image_info in enumerate(sampled_images):
        image_id = image_info['id']
        image_name = image_info['file_name']
        image_path = os.path.join(image_dir, image_name)
        
        logger.info(f"Processing {i+1}/{num_samples}: {image_name}")
        
        # Load image
        image = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
        if image is None:
            logger.warning(f"Could not load image: {image_path}")
            continue
        
        # Get ground truth annotations for this image
        gt_annotations = [
            ann for ann in uav_annotations['annotations'] 
            if ann['image_id'] == image_id
        ]
        
        # Run inference
        try:
            # Preprocess image
            input_tensor = evaluator.preprocess_image(image)
            
            # Run inference
            dets, labels = evaluator.infer(input_tensor)
            
            # Postprocess detections
            detections = evaluator.postprocess_detections(
                dets, labels, (image_info['width'], image_info['height'])
            )
            
            # Debug: Print raw model outputs for first few images
            if i < 3:
                logger.info(f"  - Raw dets shape: {dets.shape}, range: [{dets.min():.3f}, {dets.max():.3f}]")
                logger.info(f"  - Raw labels shape: {labels.shape}, range: [{labels.min():.3f}, {labels.max():.3f}]")
                
                # Calculate probabilities for debugging
                logits = labels[0, :, :]  # (300, 2)
                probs = np.exp(logits[:, 0]) / (np.exp(logits[:, 0]) + np.exp(logits[:, 1]))
                logger.info(f"  - UAV probabilities range: [{probs.min():.3f}, {probs.max():.3f}]")
                logger.info(f"  - Top 5 UAV probabilities: {sorted(probs, reverse=True)[:5]}")
                logger.info(f"  - Detections above 0.5 threshold: {np.sum(probs > 0.5)}")
            
            # Add image_id to predictions
            for det in detections:
                det['image_id'] = image_id
            
            all_predictions.extend(detections)
            
            # Add ground truth to list
            for gt in gt_annotations:
                gt['image_id'] = image_id
            all_ground_truth.extend(gt_annotations)
            
            # Visualize detections
            vis_path = os.path.join(output_dir, "visualizations", f"{image_name}")
            evaluator.visualize_detections(image, detections, vis_path, gt_annotations)
            
            logger.info(f"  - Detections: {len(detections)}, Ground truth: {len(gt_annotations)}")
            
        except Exception as e:
            logger.error(f"Error processing {image_name}: {e}")
            continue
    
    # Calculate metrics
    logger.info("Calculating metrics...")
    
    # Create ground truth dictionary for COCO evaluation
    gt_dict = {
        'info': {
            'description': 'UAV Detection Evaluation',
            'version': '1.0',
            'year': 2024,
            'contributor': 'TensorRT Evaluation'
        },
        'images': sampled_images,
        'annotations': all_ground_truth,
        'categories': uav_annotations['categories']
    }
    
    metrics = calculate_coco_metrics(all_predictions, gt_dict)
    
    # Print results
    logger.info("=" * 60)
    logger.info("EVALUATION RESULTS - UAV CLASS")
    logger.info("=" * 60)
    logger.info(f"IoU metric: bbox")
    logger.info(f" Average Precision  (AP) @[ IoU=0.50:0.95 | area=   all | maxDets=100 ] = {metrics['AP']:.3f}")
    logger.info(f" Average Precision  (AP) @[ IoU=0.50      | area=   all | maxDets=100 ] = {metrics['AP50']:.3f}")
    logger.info(f" Average Precision  (AP) @[ IoU=0.75      | area=   all | maxDets=100 ] = {metrics['AP75']:.3f}")
    logger.info(f" Average Precision  (AP) @[ IoU=0.50:0.95 | area= small | maxDets=100 ] = {metrics['APs']:.3f}")
    logger.info(f" Average Precision  (AP) @[ IoU=0.50:0.95 | area=medium | maxDets=100 ] = {metrics['APm']:.3f}")
    logger.info(f" Average Precision  (AP) @[ IoU=0.50:0.95 | area= large | maxDets=100 ] = {metrics['APl']:.3f}")
    logger.info(f" Average Recall     (AR) @[ IoU=0.50:0.95 | area=   all | maxDets=  1 ] = {metrics['AR1']:.3f}")
    logger.info(f" Average Recall     (AR) @[ IoU=0.50:0.95 | area=   all | maxDets= 10 ] = {metrics['AR10']:.3f}")
    logger.info(f" Average Recall     (AR) @[ IoU=0.50:0.95 | area=   all | maxDets=100 ] = {metrics['AR100']:.3f}")
    logger.info(f" Average Recall     (AR) @[ IoU=0.50:0.95 | area= small | maxDets=100 ] = {metrics['ARs']:.3f}")
    logger.info(f" Average Recall     (AR) @[ IoU=0.50:0.95 | area=medium | maxDets=100 ] = {metrics['ARm']:.3f}")
    logger.info(f" Average Recall     (AR) @[ IoU=0.50:0.95 | area= large | maxDets=100 ] = {metrics['ARl']:.3f}")
    logger.info("=" * 60)
    
    # Save results
    results = {
        'metrics': metrics,
        'predictions': all_predictions,
        'ground_truth': all_ground_truth,
        'config': {
            'engine_path': engine_path,
            'sample_percentage': sample_percentage,
            'num_images_processed': len(sampled_images),
            'confidence_threshold': evaluator.confidence_threshold,
            'nms_threshold': evaluator.nms_threshold
        }
    }
    
    results_path = os.path.join(output_dir, "evaluation_results.json")
    with open(results_path, 'w') as f:
        json.dump(results, f, indent=2)
    
    logger.info(f"Results saved to: {results_path}")
    logger.info(f"Visualizations saved to: {os.path.join(output_dir, 'visualizations')}")
    
    return 0

if __name__ == "__main__":
    exit(main())
