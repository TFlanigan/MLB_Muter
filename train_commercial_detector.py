#!/usr/bin/env python3
"""
Training script for ESP32-CAM commercial detection.
Run this on your PC to generate feature arrays for the ESP32 code.

Usage:
1. Collect grayscale images of commercials and programs (160x120, QQVGA)
2. Place them in 'commercial/' and 'program/' folders
3. Run: python train_commercial_detector.py
4. Copy the generated arrays into main.cpp
"""

import cv2
import numpy as np
import os
import json

FEATURE_VECTOR_SIZE = 17  # 16 histogram bins + 1 edge density
TRAINING_SAMPLES_PER_CLASS = 5

def extract_features(image_path):
    """Extract features from a single image."""
    img = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
    if img is None:
        return None

    height, width = img.shape
    hist = np.zeros(16, dtype=np.uint32)
    edge_count = 0
    total_pixels = width * height

    # Compute histogram and edge detection
    for y in range(1, height - 1):
        for x in range(1, width - 1):
            center = img[y, x]
            left = img[y, x - 1]
            right = img[y, x + 1]
            up = img[y - 1, x]
            down = img[y + 1, x]

            # Histogram bin
            bin_idx = center // 16
            hist[bin_idx] += 1

            # Edge detection
            grad_x = abs(int(right) - int(left))
            grad_y = abs(int(down) - int(up))
            if grad_x + grad_y > 50:
                edge_count += 1

    # Normalize features
    features = np.zeros(FEATURE_VECTOR_SIZE, dtype=np.float32)
    features[:16] = hist.astype(np.float32) / total_pixels
    features[16] = float(edge_count) / total_pixels

    return features

def load_training_data(folder_path, max_samples=TRAINING_SAMPLES_PER_CLASS):
    """Load and extract features from training images."""
    features_list = []
    if not os.path.exists(folder_path):
        print(f"Warning: {folder_path} does not exist")
        return features_list

    image_files = [f for f in os.listdir(folder_path)
                   if f.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp'))]

    for filename in image_files[:max_samples]:
        filepath = os.path.join(folder_path, filename)
        features = extract_features(filepath)
        if features is not None:
            features_list.append(features)
            print(f"Processed {filename}")

    return features_list

def generate_cpp_arrays(commercial_features, program_features):
    """Generate C++ array declarations."""
    def array_to_string(features_list, name):
        lines = [f"static float {name}[{len(features_list)}][{FEATURE_VECTOR_SIZE}] = {{"]
        for i, features in enumerate(features_list):
            line = "    {" + ", ".join(f"{x:.6f}f" for x in features) + "}"
            if i < len(features_list) - 1:
                line += ","
            lines.append(line)
        lines.append("};")
        return "\n".join(lines)

    commercial_array = array_to_string(commercial_features, "commercialFeatures")
    program_array = array_to_string(program_features, "programFeatures")

    return commercial_array, program_array

def main():
    print("ESP32-CAM Commercial Detection Training")
    print("=" * 40)

    # Load training data
    commercial_features = load_training_data("commercial")
    program_features = load_training_data("program")

    if len(commercial_features) == 0 or len(program_features) == 0:
        print("Error: Need training images in 'commercial/' and 'program/' folders")
        return

    print(f"\nLoaded {len(commercial_features)} commercial samples")
    print(f"Loaded {len(program_features)} program samples")

    # Generate C++ code
    commercial_array, program_array = generate_cpp_arrays(commercial_features, program_features)

    # Save to file
    with open("trained_features.cpp", "w") as f:
        f.write("// Generated training features - copy into main.cpp\n\n")
        f.write(commercial_array)
        f.write("\n\n")
        f.write(program_array)
        f.write("\n")

    print("\nGenerated trained_features.cpp")
    print("Copy the arrays into your ESP32 main.cpp file")

    # Also save as JSON for reference
    training_data = {
        'commercial': [feat.tolist() for feat in commercial_features],
        'program': [feat.tolist() for feat in program_features]
    }

    with open("training_features.json", "w") as f:
        json.dump(training_data, f, indent=2)

    print("Saved training_features.json for reference")

if __name__ == "__main__":
    main()