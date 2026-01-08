# Machine Learning Module (`ml/`)

Classical machine learning algorithms optimized for embedded systems.

## Algorithms

| Category | Algorithms |
|----------|------------|
| **Classification** | Decision Tree, Random Forest, Naive Bayes, Logistic Regression, SVM |
| **Clustering** | K-Means, Mini-batch K-Means, DBSCAN |
| **Regression** | Linear Regression, Decision Tree Regression |
| **Ensemble** | AdaBoost, Random Forest |
| **Anomaly** | Isolation Forest |

## Features

### Decision Trees
- Classification and regression
- Gini/Entropy splitting
- Max depth control
- Min samples per leaf

### Random Forest
- Bagging ensemble
- Feature sampling
- OOB error estimation

### SVM (Linear)
- Pegasos algorithm
- Binary classification
- Kernel-free (linear)

### Naive Bayes
- Gaussian Naive Bayes
- Multi-class support
- Probability output

### DBSCAN
- Density-based clustering
- Automatic cluster count
- Noise detection

### Isolation Forest
- Anomaly detection
- Outlier scoring
- Streaming support

## Usage

```c
#include "eif_ml.h"

// Decision Tree
eif_dtree_t tree;
eif_dtree_init(&tree, 10, 2, &pool);
eif_dtree_fit(&tree, X, y, n_samples, n_features);
int pred = eif_dtree_predict(&tree, x);

// Random Forest
eif_rforest_t rf;
eif_rforest_init(&rf, 10, 5, 2, &pool);
eif_rforest_fit(&rf, X, y, n_samples, n_features);
int pred = eif_rforest_predict(&rf, x);

// SVM
eif_svm_t svm;
eif_svm_init(&svm, n_features, 0.01, 0.001, &pool);
eif_svm_fit(&svm, X, y, n_samples);
int pred = eif_svm_predict(&svm, x);
```

## Files
- `eif_ml_trees.c` - Decision Tree, Random Forest
- `eif_ml_bayes.c` - Naive Bayes
- `eif_ml_svm.c` - Linear SVM
- `eif_ml_cluster.c` - K-Means, DBSCAN
- `eif_ml_linear.c` - Linear/Logistic Regression
