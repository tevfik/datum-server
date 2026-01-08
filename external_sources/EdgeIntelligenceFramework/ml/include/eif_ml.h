/**
 * @file eif_ml.h
 * @brief EIF Machine Learning Algorithms
 *
 * Contains:
 * - Isolation Forest (Anomaly Detection)
 * - Decision Tree (Classification & Regression)
 * - Logistic Regression (Binary & Multiclass)
 * - Simple Linear Regression
 * - K-Means (Batch and Online)
 * - DBSCAN
 * - Random Forest
 * - Naive Bayes
 */

#ifndef EIF_ML_H
#define EIF_ML_H

#include "eif_memory.h"
#include "eif_ml_knn.h"
#include "eif_status.h"
#include "eif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Isolation Forest (Anomaly Detection)
// ============================================================================

typedef struct {
  int split_feature;
  float32_t split_value;
  int left;
  int right;
  int size;
} eif_iforest_node_t;

typedef struct {
  int num_trees;
  int max_samples;
  int max_depth;
  int num_features;
  int max_nodes_per_tree;
  eif_iforest_node_t *nodes;
  int *tree_offsets;
  int *tree_sizes;
  float32_t c_factor;
} eif_iforest_t;

eif_status_t eif_iforest_init(eif_iforest_t *forest, int num_trees,
                              int max_samples, int max_depth, int num_features,
                              eif_memory_pool_t *pool);
eif_status_t eif_iforest_fit(eif_iforest_t *forest, const float32_t *data,
                             int num_samples, eif_memory_pool_t *pool);
float32_t eif_iforest_score(const eif_iforest_t *forest,
                            const float32_t *sample);
float32_t eif_iforest_predict(const eif_iforest_t *forest,
                              const float32_t *sample, float32_t threshold);

// ============================================================================
// Decision Tree (Classification & Regression)
// ============================================================================

#define EIF_DTREE_MAX_DEPTH 15

typedef enum {
  EIF_DTREE_CLASSIFICATION,
  EIF_DTREE_REGRESSION
} eif_dtree_type_t;

typedef struct eif_dtree_node {
  int split_feature;
  float32_t split_value;
  float32_t
      value; // For regression, mean value; for classification, majority class
  int *class_counts; // [num_classes] vote counts for classification
  int left;
  int right;
  int num_samples;
} eif_dtree_node_t;

typedef struct {
  eif_dtree_type_t type;
  int max_depth;
  int min_samples_split;
  int min_samples_leaf;
  int num_features;
  int num_classes;
  eif_dtree_node_t *nodes;
  int num_nodes;
  int max_nodes;
  float32_t *feature_importance;
} eif_dtree_t;

eif_status_t eif_dtree_init(eif_dtree_t *tree, eif_dtree_type_t type,
                            int max_depth, int min_samples_split,
                            int min_samples_leaf, int num_features,
                            int num_classes, eif_memory_pool_t *pool);
eif_status_t eif_dtree_fit(eif_dtree_t *tree, const float32_t *X,
                           const float32_t *y, int num_samples,
                           eif_memory_pool_t *pool);
float32_t eif_dtree_predict(const eif_dtree_t *tree, const float32_t *x);
int eif_dtree_predict_class(const eif_dtree_t *tree, const float32_t *x);

// ============================================================================
// Random Forest (Ensemble)
// ============================================================================

#define EIF_RFOREST_MAX_TREES 100

typedef struct {
  int num_trees;
  int max_depth;
  int max_features;
  int min_samples_split;
  float32_t sample_ratio;
} eif_rforest_config_t;

typedef struct {
  eif_rforest_config_t config;
  eif_dtree_t *trees;
  eif_dtree_type_t type;
  int num_features;
  int num_classes;
  float32_t *feature_importance;
} eif_rforest_t;

eif_status_t eif_rforest_init(eif_rforest_t *forest,
                              eif_rforest_config_t *config,
                              eif_dtree_type_t type, int num_features,
                              int num_classes, eif_memory_pool_t *pool);
eif_status_t eif_rforest_fit(eif_rforest_t *forest, const float32_t *X,
                             const float32_t *y, int num_samples,
                             eif_memory_pool_t *pool);
int eif_rforest_predict_class(const eif_rforest_t *forest, const float32_t *x);
float32_t eif_rforest_predict(const eif_rforest_t *forest, const float32_t *x);

// ============================================================================
// Gaussian Naive Bayes
// ============================================================================

typedef struct {
  int num_features;
  int num_classes;
  float32_t *class_priors; // [num_classes]
  float32_t *means;
  float32_t *variances;
  int *class_counts;
  int total_samples;
} eif_naive_bayes_t;

eif_status_t eif_nb_init(eif_naive_bayes_t *model, int num_features,
                         int num_classes, eif_memory_pool_t *pool);
eif_status_t eif_nb_fit(eif_naive_bayes_t *model, const float32_t *X,
                        const int *y, int num_samples);
int eif_nb_predict(const eif_naive_bayes_t *model, const float32_t *x);
eif_status_t eif_nb_predict_proba(const eif_naive_bayes_t *model,
                                  const float32_t *x, float32_t *probabilities);

// ============================================================================
// Logistic Regression (Binary)
// ============================================================================

typedef struct {
  int num_features;
  float32_t *weights;
  float32_t learning_rate;
  float32_t regularization;
} eif_logreg_t;

eif_status_t eif_logreg_init(eif_logreg_t *model, int num_features,
                             float32_t learning_rate, float32_t regularization,
                             eif_memory_pool_t *pool);
eif_status_t eif_logreg_fit(eif_logreg_t *model, const float32_t *X,
                            const int *y, int num_samples, int max_epochs);
float32_t eif_logreg_predict_proba(const eif_logreg_t *model,
                                   const float32_t *x);
int eif_logreg_predict(const eif_logreg_t *model, const float32_t *x);
eif_status_t eif_logreg_update(eif_logreg_t *model, const float32_t *x, int y);

// ============================================================================
// Logistic Regression (Multiclass - One-vs-Rest)
// ============================================================================

typedef struct {
  int num_classes;
  int num_features;
  eif_logreg_t *classifiers;
} eif_logreg_multiclass_t;

eif_status_t eif_logreg_multiclass_init(eif_logreg_multiclass_t *model,
                                        int num_features, int num_classes,
                                        float32_t learning_rate,
                                        float32_t regularization,
                                        eif_memory_pool_t *pool);
eif_status_t eif_logreg_multiclass_fit(eif_logreg_multiclass_t *model,
                                       const float32_t *X, const int *y,
                                       int num_samples, int max_epochs);
int eif_logreg_multiclass_predict(const eif_logreg_multiclass_t *model,
                                  const float32_t *x);

// ============================================================================
// K-Means Clustering (Batch)
// ============================================================================

typedef struct {
  int k;
  int max_iterations;
  float32_t epsilon;
} eif_kmeans_config_t;

typedef struct {
  int k;
  int num_features;
  float32_t *centroids;
  int *labels;
  int max_iter;
  float32_t tolerance;
} eif_kmeans_t;

eif_status_t eif_kmeans_compute(const eif_kmeans_config_t *config,
                                const float32_t *input, int num_samples,
                                int num_features, float32_t *centroids,
                                int *assignments, eif_memory_pool_t *pool);
eif_status_t eif_kmeans_init(eif_kmeans_t *model, int k, int num_features,
                             int max_iter, float32_t tolerance,
                             eif_memory_pool_t *pool);
eif_status_t eif_kmeans_fit(eif_kmeans_t *model, const float32_t *data,
                            int num_samples);
int eif_kmeans_predict(const eif_kmeans_t *model, const float32_t *sample);

// ============================================================================
// K-Means Online (Incremental)
// ============================================================================

typedef struct {
  int k;
  int num_features;
  float32_t *centroids;
  int *counts;
} eif_kmeans_online_t;

eif_status_t eif_kmeans_online_init(eif_kmeans_online_t *model, int k,
                                    int num_features, eif_memory_pool_t *pool);
eif_status_t eif_kmeans_online_update(eif_kmeans_online_t *model,
                                      const float32_t *sample,
                                      float32_t learning_rate);
int eif_kmeans_online_predict(const eif_kmeans_online_t *model,
                              const float32_t *sample);

// ============================================================================
// Linear Regression
// ============================================================================

typedef struct {
  float32_t slope;
  float32_t intercept;
} eif_linreg_model_t;

typedef struct {
  float32_t slope;
  float32_t intercept;
  float32_t r_squared;
} eif_linreg_t;

eif_status_t eif_linreg_fit_simple(const float32_t *x, const float32_t *y,
                                   int num_samples, eif_linreg_model_t *model);
float32_t eif_linreg_predict_simple(const eif_linreg_model_t *model,
                                    float32_t x);
eif_status_t eif_linreg_fit(eif_linreg_t *model, const float32_t *x,
                            const float32_t *y, int num_samples);
float32_t eif_linreg_predict(const eif_linreg_t *model, float32_t x);

// ============================================================================
// Online Linear Regression (RLS)
// ============================================================================

typedef struct {
  int num_features;
  float32_t *weights;
  float32_t *P;
  float32_t lambda;
} eif_linreg_online_t;

typedef struct {
  int num_features;
  float32_t *weights;
  float32_t *P;
  float32_t lambda;
} eif_rls_t;

eif_status_t eif_linreg_online_init(eif_linreg_online_t *model,
                                    int num_features, float32_t lambda,
                                    eif_memory_pool_t *pool);
eif_status_t eif_linreg_online_update(eif_linreg_online_t *model,
                                      const float32_t *x, float32_t y,
                                      eif_memory_pool_t *pool);
float32_t eif_linreg_online_predict(const eif_linreg_online_t *model,
                                    const float32_t *x);

eif_status_t eif_rls_init(eif_rls_t *model, int num_features, float32_t lambda,
                          eif_memory_pool_t *pool);
eif_status_t eif_rls_update(eif_rls_t *model, const float32_t *x, float32_t y);
float32_t eif_rls_predict(const eif_rls_t *model, const float32_t *x);

// ============================================================================
// DBSCAN (Density-Based Clustering)
// ============================================================================

#define EIF_DBSCAN_UNDEFINED -2
#define EIF_DBSCAN_NOISE -1

typedef struct {
  float32_t *core_points;
  int *labels;
  int num_clusters;
  int num_noise;
  int num_samples;
} eif_dbscan_result_t;

eif_status_t eif_dbscan_compute(const float32_t *data, int num_samples,
                                int num_features, float32_t eps, int min_points,
                                eif_dbscan_result_t *result,
                                eif_memory_pool_t *pool);

// Alias for backward compatibility
#define eif_dbscan eif_dbscan_compute

int eif_dbscan_predict(const float32_t *data, const eif_dbscan_result_t *result,
                       int num_features, float32_t eps,
                       const float32_t *new_point);

// Distance helper
float32_t eif_distance_euclidean(const float32_t *a, const float32_t *b,
                                 int dim);

// ============================================================================
// Linear SVM (Support Vector Machine)
// ============================================================================

typedef struct {
  int num_features;
  float32_t *weights;
  float32_t bias;
  float32_t lambda; // Regularization parameter
  int t;            // Iteration counter for learning rate
} eif_svm_t;

eif_status_t eif_svm_init(eif_svm_t *svm, int num_features, float32_t lambda,
                          eif_memory_pool_t *pool);
eif_status_t eif_svm_fit(eif_svm_t *svm, const float32_t *X, const int *y,
                         int num_samples, int max_epochs);
eif_status_t eif_svm_update(eif_svm_t *svm, const float32_t *x, int y);
float32_t eif_svm_decision(const eif_svm_t *svm, const float32_t *x);
int eif_svm_predict(const eif_svm_t *svm, const float32_t *x);

// ============================================================================
// AdaBoost (Adaptive Boosting)
// ============================================================================

typedef struct {
  int feature_idx;
  float32_t threshold;
  int polarity; // 1 or -1
} eif_stump_t;

typedef struct {
  int num_estimators;
  int num_features;
  int num_fitted;
  eif_stump_t *stumps;
  float32_t *alphas; // Classifier weights
} eif_adaboost_t;

eif_status_t eif_adaboost_init(eif_adaboost_t *model, int num_estimators,
                               int num_features, eif_memory_pool_t *pool);
eif_status_t eif_adaboost_fit(eif_adaboost_t *model, const float32_t *X,
                              const int *y, int num_samples,
                              eif_memory_pool_t *pool);
int eif_adaboost_predict(const eif_adaboost_t *model, const float32_t *x);

// ============================================================================
// Mini-Batch K-Means (for streaming/online)
// ============================================================================

typedef struct {
  int k;
  int num_features;
  int batch_size;
  float32_t *centroids;
  int *counts;
  bool initialized;
} eif_minibatch_kmeans_t;

eif_status_t eif_minibatch_kmeans_init(eif_minibatch_kmeans_t *model, int k,
                                       int num_features, int batch_size,
                                       eif_memory_pool_t *pool);
eif_status_t eif_minibatch_kmeans_partial_fit(eif_minibatch_kmeans_t *model,
                                              const float32_t *X,
                                              int num_samples);
int eif_minibatch_kmeans_predict(const eif_minibatch_kmeans_t *model,
                                 const float32_t *x);

#ifdef __cplusplus
}
#endif

#endif // EIF_ML_H
