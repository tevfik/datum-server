/**
 * @file main.c
 * @brief k-Nearest Neighbors Demo (Iris Classification)
 *
 * Classifies a flower sample using the k-NN algorithm on a subset
 * of the Iris dataset.
 */

#include "eif_ml.h"
#include <stdio.h>

// Tiny training set: 5 Setosa (0), 5 Versicolor (1), 5 Virginica (2)
// Features: Sepal Length, Sepal Width, Petal Length, Petal Width
float32_t train_data[] = {
    // Setosa
    5.1f, 3.5f, 1.4f, 0.2f, 4.9f, 3.0f, 1.4f, 0.2f, 4.7f, 3.2f, 1.3f, 0.2f,
    4.6f, 3.1f, 1.5f, 0.2f, 5.0f, 3.6f, 1.4f, 0.2f,
    // Versicolor
    7.0f, 3.2f, 4.7f, 1.4f, 6.4f, 3.2f, 4.5f, 1.5f, 6.9f, 3.1f, 4.9f, 1.5f,
    5.5f, 2.3f, 4.0f, 1.3f, 6.5f, 2.8f, 4.6f, 1.5f,
    // Virginica
    6.3f, 3.3f, 6.0f, 2.5f, 5.8f, 2.7f, 5.1f, 1.9f, 7.1f, 3.0f, 5.9f, 2.1f,
    6.3f, 2.9f, 5.6f, 1.8f, 6.5f, 3.0f, 5.8f, 2.2f};

int32_t train_labels[] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2};

int main() {
  // 1. Initialize k-NN
  eif_ml_knn_t knn;
  // Data, Labels, NumSamples, NumFeatures, k
  eif_ml_knn_init(&knn, train_data, train_labels, 15, 4, 3);

  // 2. Define Test Samples
  float32_t test_setosa[] = {5.2f, 3.4f, 1.4f, 0.2f};     // Should be 0
  float32_t test_versicolor[] = {6.0f, 2.9f, 4.5f, 1.5f}; // Should be 1
  float32_t test_virginica[] = {6.8f, 3.2f, 5.9f, 2.3f};  // Should be 2

  // 3. Predict & Output
  int p0 = eif_ml_knn_predict(&knn, test_setosa);
  int p1 = eif_ml_knn_predict(&knn, test_versicolor);
  int p2 = eif_ml_knn_predict(&knn, test_virginica);

  // JSON Output
  printf("{\n");
  printf("  \"results\": [\n");

  printf("    { \"input\": \"Setosa-like\", \"features\": [5.2, 3.4, 1.4, "
         "0.2], \"prediction\": %d, \"expected\": 0 },\n",
         p0);
  printf("    { \"input\": \"Versicolor-like\", \"features\": [6.0, 2.9, 4.5, "
         "1.5], \"prediction\": %d, \"expected\": 1 },\n",
         p1);
  printf("    { \"input\": \"Virginica-like\", \"features\": [6.8, 3.2, 5.9, "
         "2.3], \"prediction\": %d, \"expected\": 2 }\n",
         p2);

  printf("  ],\n");
  printf("  \"info\": \"k-NN Demo: Classifying Iris flowers using k=3 on "
         "embedded data.\"\n");
  printf("}\n");

  return 0;
}
