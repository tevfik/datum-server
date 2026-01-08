# Anomaly Detection Demo - Industrial Sensor Monitoring

## Overview
This tutorial demonstrates **anomaly detection** for industrial IoT using statistical methods and clustering to identify equipment failures.

## Scenario
A factory monitors temperature and vibration sensors on machinery. Normal operation falls within expected ranges, while anomalies indicate potential failures:
- **Overheating** - Temperature spikes above normal
- **High Vibration** - Bearing or alignment issues
- **Combined** - Multiple simultaneous faults

## Algorithms Used

### 1. Z-Score Anomaly Detection
Uses online statistics to detect outliers based on standard deviations from mean.

**Welford's Online Algorithm:**
```c
// Update mean and variance incrementally (no need to store all data)
delta = x - mean
mean += delta / n
M2 += delta * (x - mean)
variance = M2 / (n - 1)
```

**Z-Score Calculation:**
```
z = |x - μ| / σ
```

**Anomaly Rule:**
```
IF z > threshold (e.g., 2.5):
    → ANOMALY
```

| Z-Score | Percentile | Interpretation |
|---------|------------|----------------|
| 1.0 | 68% | Normal variation |
| 2.0 | 95% | Unusual |
| 2.5 | 99% | Likely anomaly |
| 3.0 | 99.7% | Definite anomaly |

### 2. K-Means Clustering
Unsupervised learning to group similar data points.

**Algorithm:**
1. Initialize K centroids randomly
2. Assign each point to nearest centroid
3. Update centroids to cluster means
4. Repeat until convergence

**EIF API:**
```c
eif_kmeans_config_t cfg = {
    .k = 2,
    .max_iterations = 20,
    .epsilon = 0.001f
};
eif_kmeans_compute(&cfg, data, n_samples, n_features, 
                   centroids, labels, &pool);
```

**Anomaly Detection with K-Means:**
- Cluster data into K groups
- Identify smallest cluster as "anomaly cluster"
- Points in anomaly cluster are flagged

## Demo Walkthrough

1. **Data Generation** - 100 samples with 10% injected anomalies
2. **Z-Score Method** - Train on normal data, test on all
3. **Scatter Plot** - Visualize temperature vs vibration
4. **K-Means Method** - Cluster analysis
5. **Comparison** - Precision/Recall metrics for both methods

## Performance Metrics

| Metric | Formula | Meaning |
|--------|---------|---------|
| **Precision** | TP / (TP + FP) | Of detected, how many are real anomalies |
| **Recall** | TP / (TP + FN) | Of real anomalies, how many detected |
| **F1 Score** | 2 × P × R / (P + R) | Harmonic mean of precision and recall |

## Key EIF Functions

| Function | Purpose |
|----------|---------|
| `eif_kmeans_compute()` | Run K-Means clustering |
| `eif_online_stats_update()` | Welford's online statistics |
| `eif_memory_alloc()` | Memory pool allocation |

## Method Comparison

| Aspect | Z-Score | K-Means |
|--------|---------|---------|
| **Training** | Needs labeled normal data | Unsupervised |
| **Assumption** | Normal distribution | Cluster separability |
| **Computation** | O(1) per point | O(n×k×iter) |
| **Multivariate** | Independent per feature | Natural multivariate |
| **Online** | Yes | Batch (or mini-batch) |

## Real-World Applications
- Predictive maintenance in manufacturing
- Network intrusion detection
- Credit card fraud detection
- Medical device monitoring
- Quality control in production

## Run the Demo
```bash
cd build && ./bin/anomaly_detector_demo
```
