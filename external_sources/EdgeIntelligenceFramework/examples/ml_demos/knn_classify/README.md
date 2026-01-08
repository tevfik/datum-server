# k-Nearest Neighbors Demo (Iris Classifier)

This demo classifies flower samples using the k-Nearest Neighbors (k-NN) algorithm.

## Algorithm
- **k-NN (Lazy Learning)**: Stores reference data points in memory.
- **Classification**: When a new sample arrives, it finds the `k` (3) closest training points (Euclidean distance) and assigns the most common label among them.
- **Dataset**: A tiny subset (15 samples) of the Iris dataset.

## Usage
Run the demo:
```bash
./bin/knn_classify_demo
```

## Expected Output
A JSON object containing prediction results for three test samples (one for each class):
```json
{
  "results": [
    { "input": "Setosa-like", ..., "prediction": 0, "expected": 0 },
    { "input": "Versicolor-like", ..., "prediction": 1, "expected": 1 },
    { "input": "Virginica-like", ..., "prediction": 2, "expected": 2 }
  ]
}
```
