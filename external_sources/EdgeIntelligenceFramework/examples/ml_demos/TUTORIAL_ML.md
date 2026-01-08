# Machine Learning Algorithms Tutorial

This tutorial covers classical machine learning algorithms in the Edge Intelligence Framework.

## Table of Contents
1. [Overview](#overview)
2. [Naive Bayes](#naive-bayes)
3. [Logistic Regression](#logistic-regression)
4. [Support Vector Machine](#support-vector-machine)
5. [Random Forest](#random-forest)
6. [Gradient Boosting](#gradient-boosting)
7. [Choosing the Right Algorithm](#choosing-the-right-algorithm)

---

## Overview

All ML algorithms in EIF are designed for **inference only**. Training happens offline (Python, MATLAB, etc.), and the trained model parameters are loaded into these lightweight C structures.

### Common Pattern

```c
// 1. Initialize
eif_xxx_t model;
eif_xxx_init(&model, num_features, num_classes);

// 2. Set weights/parameters (from trained model)
model.weights = trained_weights;
model.bias = trained_bias;

// 3. Predict
int class = eif_xxx_predict(&model, features);
```

---

## Naive Bayes

### When to Use
- Text classification
- Spam filtering
- Simple, interpretable model

### Gaussian Naive Bayes

```c
#include "eif_ml_naive_bayes.h"

eif_gaussian_nb_t nb;
eif_gaussian_nb_init(&nb, 4, 3);  // 4 features, 3 classes

// Set parameters (from training)
nb.means = trained_means;        // [num_classes x num_features]
nb.variances = trained_vars;
nb.log_priors = trained_priors;  // [num_classes]

// Predict
float features[4] = {5.1, 3.5, 1.4, 0.2};
int class = eif_gaussian_nb_predict(&nb, features);

// Get probabilities
float probs[3];
eif_gaussian_nb_predict_proba(&nb, features, probs);
```

### Multinomial Naive Bayes (Text)

```c
eif_multinomial_nb_t nb;
// ... for word count features
int class = eif_multinomial_nb_predict(&nb, word_counts);
```

---

## Logistic Regression

### When to Use
- Binary classification
- Probability estimates needed
- Linear decision boundary

### Binary Classification

```c
#include "eif_ml_logistic.h"

eif_binary_logistic_t lr;
eif_binary_logistic_init(&lr, 10);  // 10 features

// Set parameters
lr.weights = trained_weights;  // [num_features]
lr.bias = trained_bias;

// Predict
float prob = eif_binary_logistic_proba(&lr, features);  // 0.0-1.0
int class = eif_binary_logistic_predict(&lr, features); // 0 or 1
```

### Multi-class (Softmax)

```c
eif_softmax_regression_t sr;
eif_softmax_regression_init(&sr, 10, 5);  // 10 features, 5 classes

sr.weights = trained_weights;  // [num_classes x num_features]
sr.bias = trained_bias;        // [num_classes]

float probs[5];
eif_softmax_regression_proba(&sr, features, probs);
int class = eif_softmax_regression_predict(&sr, features);
```

---

## Support Vector Machine

### When to Use
- High-dimensional data
- Clear margin between classes
- Non-linear boundaries (RBF kernel)

### Linear SVM

```c
#include "eif_ml_svm.h"

eif_linear_svm_t svm;
eif_linear_svm_init(&svm, 10, 3);  // 10 features, 3 classes

// Set parameters (one-vs-all weights)
svm.weights = trained_weights;  // [num_classes x num_features]
svm.bias = trained_bias;

int class = eif_linear_svm_predict(&svm, features);
```

### RBF Kernel SVM

```c
eif_rbf_svm_t svm;
svm.num_features = 10;
svm.num_sv = 50;                // 50 support vectors
svm.gamma = 0.1f;               // RBF parameter
svm.support_vectors = sv_data;  // [num_sv x num_features]
svm.alpha_y = alpha_values;     // [num_sv]

float decision = eif_rbf_svm_decision(&svm, features);
int class = eif_rbf_svm_predict(&svm, features);
```

---

## Random Forest

### When to Use
- Robust to outliers
- Feature importance analysis
- Ensemble for better accuracy

### Usage

```c
#include "eif_ml_random_forest.h"

eif_random_forest_t rf;
eif_rf_init(&rf, 10, 3);  // 10 features, 3 classes

// Trees are built programmatically or loaded from file
// Example with pre-built trees:
eif_rf_create_example(&rf, 10, 3);

// Predict
int class = eif_rf_predict(&rf, features);

// Get probabilities (vote proportions)
float probs[3];
eif_rf_predict_proba(&rf, features, probs);
```

---

## Gradient Boosting

### When to Use
- Highest accuracy needed
- Trained with XGBoost/LightGBM
- Additive model

### Usage

```c
#include "eif_ml_gradient_boost.h"

eif_gradient_boost_t gbm;
eif_gbm_init(&gbm, 10, 2, 0.1f);  // 10 features, 2 classes, lr=0.1

// Trees built or loaded
eif_gbm_create_example(&gbm, 10);

// Predict
float prob = eif_gbm_predict_proba_binary(&gbm, features);
int class = eif_gbm_predict(&gbm, features);
```

---

## Choosing the Right Algorithm

| Algorithm | Pros | Cons | Best For |
|-----------|------|------|----------|
| **Naive Bayes** | Fast, simple | Assumes independence | Text, quick baseline |
| **Logistic Regression** | Interpretable, probabilities | Linear only | Binary classification |
| **SVM** | Works in high dimensions | Slow with many SVs | Small datasets |
| **Random Forest** | Robust, handles non-linear | Memory for trees | General purpose |
| **Gradient Boosting** | Highest accuracy | Complex, slow to train | When accuracy matters |

### Memory Requirements

| Algorithm | Typical Memory |
|-----------|---------------|
| Naive Bayes | O(features × classes) |
| Logistic | O(features × classes) |
| Linear SVM | O(features × classes) |
| RBF SVM | O(SVs × features) |
| Random Forest | O(trees × nodes) |
| GBM | O(trees × nodes) |

---

## Converting from Python

### Scikit-learn → EIF

```python
# Python training
from sklearn.linear_model import LogisticRegression
clf = LogisticRegression()
clf.fit(X_train, y_train)

# Export weights
print(f"weights = {clf.coef_.flatten().tolist()};")
print(f"bias = {clf.intercept_[0]};")
```

Then in C:
```c
float weights[] = {0.1, -0.2, 0.3, ...};  // From Python
float bias = 0.5;

lr.weights = weights;
lr.bias = bias;
```
