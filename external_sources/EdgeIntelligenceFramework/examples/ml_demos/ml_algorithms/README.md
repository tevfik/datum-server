# ML Algorithms Demo - Phase 1 Machine Learning

## Overview
This demo showcases the Phase 1 machine learning algorithms implemented in EIF:
- **Isolation Forest** - Anomaly Detection
- **Logistic Regression** - Binary & Multiclass Classification
- **Decision Tree** - Classification & Regression
- **Time Series Extensions** - Decomposition, Change Points, ACF/PACF

## Running the Demo

```bash
./ml_algorithms_demo
```

The demo is interactive and guides you through each algorithm.

---

## 1. Isolation Forest

### Theory
Isolation Forest detects anomalies by building random trees that isolate data points. Anomalies are easier to isolate, resulting in shorter average path lengths.

**Key Concept:**
```
Anomaly Score = 2^(-E[path_length] / c(n))
```
- Score close to 1 → Anomaly
- Score close to 0.5 → Normal
- Score close to 0 → Dense region

### API
```c
eif_iforest_t forest;

// Initialize: 50 trees, 64 samples/tree, max depth 8
eif_iforest_init(&forest, 50, 64, 8, num_features, &pool);

// Train on data
eif_iforest_fit(&forest, X, num_samples, &pool);

// Score a sample (0-1, higher = more anomalous)
float score = eif_iforest_score(&forest, sample);

// Predict (1=anomaly, 0=normal)
float prediction = eif_iforest_predict(&forest, sample, 0.6f);
```

### Use Cases
- Fraud detection
- Network intrusion detection
- Manufacturing defect detection
- Sensor fault identification

---

## 2. Logistic Regression

### Theory
Logistic Regression uses the sigmoid function to model probability:

```
P(y=1|x) = σ(w·x + b) = 1 / (1 + e^-(w·x+b))
```

Training minimizes cross-entropy loss using gradient descent:
```
w = w - lr × (prediction - target) × x
```

### API

**Binary Classification:**
```c
eif_logreg_t model;

// Initialize: 2 features, lr=0.1, L2 reg=0.01
eif_logreg_init(&model, 2, 0.1f, 0.01f, &pool);

// Batch training
eif_logreg_fit(&model, X, y, num_samples, 100);

// Online update (for streaming data)
eif_logreg_update(&model, x, label);

// Predict
float prob = eif_logreg_predict_proba(&model, x);
int class = eif_logreg_predict(&model, x);
```

**Multiclass (One-vs-Rest):**
```c
eif_logreg_multiclass_t model;

// Initialize: 2 features, 3 classes
eif_logreg_multiclass_init(&model, 2, 3, 0.1f, 0.01f, &pool);

// Train and predict
eif_logreg_multiclass_fit(&model, X, y, num_samples, 100);
int class = eif_logreg_multiclass_predict(&model, x);
```

### Use Cases
- Binary classification (spam/not spam, pass/fail)
- Multi-class classification with OvR
- Online learning scenarios

---

## 3. Decision Tree

### Theory
Decision Trees recursively partition data using feature thresholds to minimize impurity.

**Classification (Gini Impurity):**
```
Gini = 1 - Σ(p_i)²
```

**Regression (MSE):**
```
MSE = Σ(y_i - mean)² / n
```

### API
```c
eif_dtree_t tree;

// Classification: max_depth=5, min_split=2, min_leaf=1, 2 features, 2 classes
eif_dtree_init(&tree, EIF_DTREE_CLASSIFICATION, 5, 2, 1, 2, 2, &pool);

// Regression
eif_dtree_init(&tree, EIF_DTREE_REGRESSION, 5, 2, 1, 2, 0, &pool);

// Train
eif_dtree_fit(&tree, X, y, num_samples, &pool);

// Predict
int class = eif_dtree_predict_class(&tree, x);
float value = eif_dtree_predict(&tree, x);

// Feature importance
for (int f = 0; f < num_features; f++) {
    printf("Feature %d importance: %.3f\n", f, tree.feature_importance[f]);
}
```

### Use Cases
- Interpretable predictions
- Feature importance analysis
- Non-linear decision boundaries

---

## 4. Time Series Extensions

### Seasonal Decomposition
Decomposes time series into Trend + Seasonal + Residual components.

```c
eif_ts_decomposition_t decomp;

// Decompose with season length 12
eif_ts_decompose(data, length, 12, &decomp, &pool);

// Access components
float* trend = decomp.trend;
float* seasonal = decomp.seasonal;
float* residual = decomp.residual;
```

### Change Point Detection
CUSUM-based detection of distributional changes.

```c
// Batch detection
int change_points[10];
int num_changes;
eif_changepoint_detect(data, length, threshold, change_points, &num_changes, 10);

// Online detection
eif_changepoint_t detector;
eif_changepoint_init(&detector, 5.0f, 0.0f);
for (int i = 0; i < length; i++) {
    if (eif_changepoint_update(&detector, data[i])) {
        printf("Change at index %d\n", i);
    }
}
```

### Autocorrelation
```c
float acf[21], pacf[21];

// ACF for lags 0-20
eif_ts_acf(data, length, acf, 20);

// PACF for lags 0-20
eif_ts_pacf(data, length, pacf, 20);
```

### Correlation Analysis
```c
// Pearson correlation coefficient
float r = eif_correlation_pearson(x, y, length);

// Spearman rank correlation
float rho = eif_correlation_spearman(x, y, length, &pool);

// Cross-correlation
float xcorr[41]; // For lags -20 to +20
eif_cross_correlation(x, y, length, xcorr, 20);
```

---

## Integration with Model Converter

For neural network-based approaches, you can combine these algorithms with converted models:

```c
// 1. Load converted model
eif_model_t nn_model;
eif_model_deserialize(&nn_model, model_buffer, model_size, &pool);

// 2. Extract features with NN
eif_neural_context_t ctx;
eif_neural_init(&ctx, &nn_model, &pool);
eif_neural_set_input(&ctx, 0, input, input_size);
eif_neural_invoke(&ctx);

float features[32];
eif_neural_get_output(&ctx, 0, features, sizeof(features));

// 3. Use ML algorithm on extracted features
int class = eif_logreg_predict(&logreg_model, features);
float anomaly_score = eif_iforest_score(&forest, features);
```

---

## Algorithm Selection Guide

| Algorithm | Best For | Online? | Interpretable? |
|-----------|----------|---------|----------------|
| Isolation Forest | Anomaly detection | No | Partial |
| Logistic Regression | Linear classification | Yes | Yes |
| Decision Tree | Non-linear, rules | No | Yes |
| K-Means | Clustering | Yes | Yes |
| KNN | Instance-based | N/A | Yes |

---

## Performance Considerations

| Algorithm | Training | Inference | Memory |
|-----------|----------|-----------|--------|
| Isolation Forest | O(n·t·log(m)) | O(t·log(m)) | O(t·nodes) |
| Logistic Regression | O(epochs·n·d) | O(d) | O(d) |
| Decision Tree | O(n·d·log(n)) | O(depth) | O(nodes) |

Where:
- n = samples, d = features, t = trees, m = subsample size

---

## Files

| File | Description |
|------|-------------|
| main.c | Demo application |
| CMakeLists.txt | Build configuration |
| README.md | This documentation |
