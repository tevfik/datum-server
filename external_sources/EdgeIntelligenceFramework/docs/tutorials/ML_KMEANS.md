# K-Means Clustering for Edge Devices

## Overview
K-Means is a popular unsupervised learning algorithm used for clustering data into `k` groups. It is useful for:
- Anomaly detection (distance from centroids)
- Data quantization / compression
- Pattern recognition (grouping sensor readings)

## API Usage

### 1. Initialization
```c
#include "eif_ml_kmeans.h"

eif_kmeans_t km = {0};
// 3 Clusters, 2 Dimensions
eif_status_t status = eif_kmeans_init(&km, 3, 2);
```

### 2. Training (On-Device)
You can train the model directly on the microcontroller using collected data.
```c
float data[] = { ... }; // Flat array [samples * dimensions]
int iterations = eif_kmeans_fit(&km, data, num_samples);
```
The algorithm uses Lloyd's algorithm with random initialization.

### 3. Inference
Assign a new sample to the nearest cluster.
```c
float sample[] = {1.2f, 0.5f};
int cluster_id = eif_kmeans_predict(&km, sample);
```

## Memory Requirements
- **Flash**: Minimal code size (< 2KB).
- **RAM**: 
  - `k * dim * sizeof(float)` for centroids.
  - Training requires access to full dataset (can be in Flash or RAM).

## Example
See `examples/ml_demos/kmeans_clustering/main.c` for a complete example generating synthetic data and clustering it.
