/**
 * @file main.c
 * @brief ML Algorithms Demo - Isolation Forest, Logistic Regression, Decision Tree
 * 
 * This tutorial demonstrates the Phase 1 machine learning algorithms:
 * 1. Isolation Forest for anomaly detection
 * 2. Logistic Regression for binary/multiclass classification
 * 3. Decision Tree for classification and regression
 * 4. Time Series extensions (decomposition, change points)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "eif_types.h"
#include "eif_memory.h"
#include "eif_data_analysis.h"
#include "../common/ascii_plot.h"

// ============================================================================
// Memory Pool
// ============================================================================

static uint8_t pool_buffer[2 * 1024 * 1024]; // 2MB pool
static eif_memory_pool_t pool;

// ============================================================================
// Data Generation Utilities
// ============================================================================

static float randf(void) {
    return (float)rand() / (float)RAND_MAX;
}

static float gaussian_noise(void) {
    // Box-Muller transform
    float u1 = randf();
    float u2 = randf();
    if (u1 < 1e-10f) u1 = 1e-10f;
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * M_PI * u2);
}

// Generate 2D data with clusters
static void generate_cluster_data(float32_t* X, int* y, int n_per_class, int num_classes) {
    float centers[][2] = {{1.0f, 1.0f}, {-1.0f, 1.0f}, {0.0f, -1.0f}};
    
    int idx = 0;
    for (int c = 0; c < num_classes; c++) {
        for (int i = 0; i < n_per_class; i++) {
            X[idx * 2] = centers[c][0] + gaussian_noise() * 0.3f;
            X[idx * 2 + 1] = centers[c][1] + gaussian_noise() * 0.3f;
            y[idx] = c;
            idx++;
        }
    }
}

// Generate data with anomalies
static void generate_anomaly_data(float32_t* X, int* labels, int n_normal, int n_anomaly) {
    // Normal data: centered at origin
    for (int i = 0; i < n_normal; i++) {
        X[i * 2] = gaussian_noise() * 0.5f;
        X[i * 2 + 1] = gaussian_noise() * 0.5f;
        labels[i] = 0;
    }
    
    // Anomalies: far from center
    for (int i = 0; i < n_anomaly; i++) {
        float angle = randf() * 2.0f * M_PI;
        float dist = 2.5f + randf() * 1.0f;
        X[(n_normal + i) * 2] = dist * cosf(angle);
        X[(n_normal + i) * 2 + 1] = dist * sinf(angle);
        labels[n_normal + i] = 1;
    }
}

// Generate time series with seasonality
static void generate_seasonal_data(float32_t* data, int length, int season) {
    for (int i = 0; i < length; i++) {
        float trend = 0.01f * i;
        float seasonal = 2.0f * sinf(2.0f * M_PI * i / (float)season);
        float noise = gaussian_noise() * 0.3f;
        data[i] = 10.0f + trend + seasonal + noise;
    }
}

// ============================================================================
// Visualization Helpers
// ============================================================================

static void plot_2d_scatter(const float32_t* X, const int* labels, int n, int width, int height) {
    char grid[25][51];
    memset(grid, '.', sizeof(grid));
    
    for (int i = 0; i < n; i++) {
        int x = (int)((X[i * 2] + 3.0f) / 6.0f * (width - 1));
        int y = (int)((X[i * 2 + 1] + 3.0f) / 6.0f * (height - 1));
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            char c = '0' + labels[i];
            if (labels[i] == 0) c = 'o';
            else if (labels[i] == 1) c = 'x';
            else if (labels[i] == 2) c = '+';
            grid[height - 1 - y][x] = c;
        }
    }
    
    printf("  ┌");
    for (int x = 0; x < width; x++) printf("─");
    printf("┐\n");
    
    for (int y = 0; y < height; y++) {
        printf("  │");
        for (int x = 0; x < width; x++) {
            char c = grid[y][x];
            if (c == 'o') printf("%s%c%s", ASCII_GREEN, c, ASCII_RESET);
            else if (c == 'x') printf("%s%c%s", ASCII_RED, c, ASCII_RESET);
            else if (c == '+') printf("%s%c%s", ASCII_BLUE, c, ASCII_RESET);
            else printf("%c", c);
        }
        printf("│\n");
    }
    
    printf("  └");
    for (int x = 0; x < width; x++) printf("─");
    printf("┘\n");
}

// ============================================================================
// Demo 1: Isolation Forest
// ============================================================================

static void demo_isolation_forest(void) {
    ascii_section("Demo 1: Isolation Forest - Anomaly Detection");
    
    printf("  Isolation Forest detects anomalies by measuring how easily\n");
    printf("  a data point can be separated from others.\n\n");
    
    printf("  %sAlgorithm:%s\n", ASCII_BOLD, ASCII_RESET);
    printf("    1. Build random trees by selecting random features/splits\n");
    printf("    2. Anomalies have shorter average path lengths\n");
    printf("    3. Score = 2^(-E[path_length] / c(n))\n\n");
    
    // Generate data
    int n_normal = 80;
    int n_anomaly = 10;
    int n_total = n_normal + n_anomaly;
    
    float32_t* X = eif_memory_alloc(&pool, n_total * 2 * sizeof(float32_t), 4);
    int* labels = eif_memory_alloc(&pool, n_total * sizeof(int), 4);
    
    generate_anomaly_data(X, labels, n_normal, n_anomaly);
    
    printf("  %sData:%s %d normal points (o), %d anomalies (x)\n\n", 
           ASCII_BOLD, ASCII_RESET, n_normal, n_anomaly);
    plot_2d_scatter(X, labels, n_total, 50, 20);
    
    printf("\n  Training Isolation Forest...\n");
    
    // Train Isolation Forest
    eif_iforest_t forest;
    eif_iforest_init(&forest, 50, 64, 8, 2, &pool);
    eif_iforest_fit(&forest, X, n_total, &pool);
    
    // Evaluate
    int tp = 0, fp = 0, tn = 0, fn = 0;
    float32_t threshold = 0.6f;
    
    printf("\n  %s┌─ Results (threshold=%.2f) ────────────────────────┐%s\n", 
           ASCII_CYAN, threshold, ASCII_RESET);
    
    for (int i = 0; i < n_total; i++) {
        float32_t score = eif_iforest_score(&forest, &X[i * 2]);
        int pred = score > threshold ? 1 : 0;
        
        if (labels[i] == 1 && pred == 1) tp++;
        else if (labels[i] == 0 && pred == 1) fp++;
        else if (labels[i] == 0 && pred == 0) tn++;
        else fn++;
        
        // Show some examples
        if (i < 5 || i >= n_normal) {
            printf("  │  Sample %2d: score=%.3f, pred=%s, true=%s │\n",
                   i, score, pred ? "anomaly" : "normal ",
                   labels[i] ? "anomaly" : "normal ");
        } else if (i == 5) {
            printf("  │  ...                                               │\n");
        }
    }
    
    float32_t precision = tp > 0 ? (float32_t)tp / (tp + fp) : 0;
    float32_t recall = tp > 0 ? (float32_t)tp / (tp + fn) : 0;
    float32_t f1 = (precision + recall) > 0 ? 2 * precision * recall / (precision + recall) : 0;
    
    printf("  │                                                     │\n");
    printf("  │  %sPrecision: %.1f%%  Recall: %.1f%%  F1: %.3f%s          │\n",
           ASCII_GREEN, precision * 100, recall * 100, f1, ASCII_RESET);
    printf("  %s└─────────────────────────────────────────────────────┘%s\n\n", 
           ASCII_CYAN, ASCII_RESET);
}

// ============================================================================
// Demo 2: Logistic Regression
// ============================================================================

static void demo_logistic_regression(void) {
    ascii_section("Demo 2: Logistic Regression - Classification");
    
    printf("  Logistic Regression uses the sigmoid function for binary\n");
    printf("  classification with gradient descent optimization.\n\n");
    
    printf("  %sFormulas:%s\n", ASCII_BOLD, ASCII_RESET);
    printf("    • P(y=1|x) = σ(w·x + b) = 1 / (1 + e^-(w·x+b))\n");
    printf("    • Loss = -Σ[y·log(p) + (1-y)·log(1-p)]\n");
    printf("    • Update: w = w - lr · ∇L\n\n");
    
    // Generate binary classification data
    int n_samples = 100;
    float32_t* X = eif_memory_alloc(&pool, n_samples * 2 * sizeof(float32_t), 4);
    int* y = eif_memory_alloc(&pool, n_samples * sizeof(int), 4);
    
    // Two clusters
    for (int i = 0; i < n_samples / 2; i++) {
        X[i * 2] = -1.0f + gaussian_noise() * 0.5f;
        X[i * 2 + 1] = -1.0f + gaussian_noise() * 0.5f;
        y[i] = 0;
    }
    for (int i = n_samples / 2; i < n_samples; i++) {
        X[i * 2] = 1.0f + gaussian_noise() * 0.5f;
        X[i * 2 + 1] = 1.0f + gaussian_noise() * 0.5f;
        y[i] = 1;
    }
    
    printf("  %sData:%s 2 classes, 50 samples each\n\n", ASCII_BOLD, ASCII_RESET);
    plot_2d_scatter(X, y, n_samples, 50, 15);
    
    // Train
    eif_logreg_t model;
    eif_logreg_init(&model, 2, 0.1f, 0.01f, &pool);
    
    printf("\n  Training Logistic Regression (100 epochs)...\n");
    eif_logreg_fit(&model, X, y, n_samples, 100);
    
    // Evaluate
    int correct = 0;
    for (int i = 0; i < n_samples; i++) {
        int pred = eif_logreg_predict(&model, &X[i * 2]);
        if (pred == y[i]) correct++;
    }
    
    printf("\n  %s┌─ Results ─────────────────────────────────────────┐%s\n", 
           ASCII_CYAN, ASCII_RESET);
    printf("  │  Weights: w1=%.3f, w2=%.3f, bias=%.3f            │\n",
           model.weights[1], model.weights[2], model.weights[0]);
    printf("  │  %sAccuracy: %d/%d = %.1f%%%s                           │\n",
           ASCII_GREEN, correct, n_samples, 100.0f * correct / n_samples, ASCII_RESET);
    printf("  %s└─────────────────────────────────────────────────────┘%s\n\n", 
           ASCII_CYAN, ASCII_RESET);
    
    // Multiclass demo
    printf("  %sMulticlass (One-vs-Rest):%s\n", ASCII_BOLD, ASCII_RESET);
    
    int n_per_class = 30;
    int num_classes = 3;
    int n_multi = n_per_class * num_classes;
    
    float32_t* X_multi = eif_memory_alloc(&pool, n_multi * 2 * sizeof(float32_t), 4);
    int* y_multi = eif_memory_alloc(&pool, n_multi * sizeof(int), 4);
    
    generate_cluster_data(X_multi, y_multi, n_per_class, num_classes);
    
    plot_2d_scatter(X_multi, y_multi, n_multi, 50, 15);
    
    eif_logreg_multiclass_t multi_model;
    eif_logreg_multiclass_init(&multi_model, 2, num_classes, 0.1f, 0.01f, &pool);
    eif_logreg_multiclass_fit(&multi_model, X_multi, y_multi, n_multi, 100);
    
    correct = 0;
    for (int i = 0; i < n_multi; i++) {
        int pred = eif_logreg_multiclass_predict(&multi_model, &X_multi[i * 2]);
        if (pred == y_multi[i]) correct++;
    }
    
    printf("\n  Multiclass Accuracy: %d/%d = %.1f%%\n\n", 
           correct, n_multi, 100.0f * correct / n_multi);
}

// ============================================================================
// Demo 3: Decision Tree
// ============================================================================

static void demo_decision_tree(void) {
    ascii_section("Demo 3: Decision Tree - Classification & Regression");
    
    printf("  Decision Trees recursively split data using feature thresholds\n");
    printf("  to create interpretable prediction rules.\n\n");
    
    printf("  %sAlgorithm (CART):%s\n", ASCII_BOLD, ASCII_RESET);
    printf("    • Classification: Minimize Gini impurity\n");
    printf("    • Regression: Minimize MSE\n");
    printf("    • Gini = 1 - Σ(p_i)²\n\n");
    
    // Classification
    int n_samples = 100;
    float32_t* X = eif_memory_alloc(&pool, n_samples * 2 * sizeof(float32_t), 4);
    float32_t* y = eif_memory_alloc(&pool, n_samples * sizeof(float32_t), 4);
    int* y_int = eif_memory_alloc(&pool, n_samples * sizeof(int), 4);
    
    // XOR-like pattern
    for (int i = 0; i < n_samples; i++) {
        X[i * 2] = randf() * 4.0f - 2.0f;
        X[i * 2 + 1] = randf() * 4.0f - 2.0f;
        int class = (X[i * 2] > 0) ^ (X[i * 2 + 1] > 0) ? 1 : 0;
        y[i] = (float32_t)class;
        y_int[i] = class;
    }
    
    printf("  %sData:%s XOR-like pattern (non-linear)\n\n", ASCII_BOLD, ASCII_RESET);
    plot_2d_scatter(X, y_int, n_samples, 50, 15);
    
    // Train
    eif_dtree_t tree;
    eif_dtree_init(&tree, EIF_DTREE_CLASSIFICATION, 5, 2, 1, 2, 2, &pool);
    
    printf("\n  Training Decision Tree (max_depth=5)...\n");
    eif_dtree_fit(&tree, X, y, n_samples, &pool);
    
    // Evaluate
    int correct = 0;
    for (int i = 0; i < n_samples; i++) {
        int pred = eif_dtree_predict_class(&tree, &X[i * 2]);
        if (pred == y_int[i]) correct++;
    }
    
    printf("\n  %s┌─ Classification Results ──────────────────────────┐%s\n", 
           ASCII_CYAN, ASCII_RESET);
    printf("  │  Tree nodes: %d                                     │\n", tree.num_nodes);
    printf("  │  Feature importance: [%.3f, %.3f]                 │\n",
           tree.feature_importance[0], tree.feature_importance[1]);
    printf("  │  %sAccuracy: %d/%d = %.1f%%%s                           │\n",
           ASCII_GREEN, correct, n_samples, 100.0f * correct / n_samples, ASCII_RESET);
    printf("  %s└─────────────────────────────────────────────────────┘%s\n\n", 
           ASCII_CYAN, ASCII_RESET);
    
    // Regression example
    printf("  %sRegression Example:%s\n", ASCII_BOLD, ASCII_RESET);
    
    for (int i = 0; i < n_samples; i++) {
        X[i * 2] = randf() * 4.0f - 2.0f;
        X[i * 2 + 1] = randf() * 4.0f - 2.0f;
        y[i] = X[i * 2] * X[i * 2] + X[i * 2 + 1]; // y = x1² + x2
    }
    
    eif_dtree_t reg_tree;
    eif_dtree_init(&reg_tree, EIF_DTREE_REGRESSION, 5, 2, 1, 2, 0, &pool);
    eif_dtree_fit(&reg_tree, X, y, n_samples, &pool);
    
    // Calculate MSE
    float32_t mse = 0;
    for (int i = 0; i < n_samples; i++) {
        float32_t pred = eif_dtree_predict(&reg_tree, &X[i * 2]);
        float32_t diff = pred - y[i];
        mse += diff * diff;
    }
    mse /= n_samples;
    
    printf("  Target: y = x1² + x2\n");
    printf("  Regression MSE: %.3f\n\n", mse);
}

// ============================================================================
// Demo 4: Time Series Extensions
// ============================================================================

static void demo_time_series(void) {
    ascii_section("Demo 4: Time Series Extensions");
    
    printf("  Extended time series analysis tools:\n");
    printf("    • Seasonal Decomposition (Trend + Season + Residual)\n");
    printf("    • Change Point Detection (CUSUM)\n");
    printf("    • Autocorrelation Analysis (ACF/PACF)\n\n");
    
    // Generate seasonal data
    int length = 100;
    int season = 12;
    float32_t* data = eif_memory_alloc(&pool, length * sizeof(float32_t), 4);
    generate_seasonal_data(data, length, season);
    
    printf("  %sGenerated Data:%s Trend + Seasonality (period=%d)\n\n", 
           ASCII_BOLD, ASCII_RESET, season);
    
    // Show data preview
    printf("  Raw data (first 24 points):\n  ");
    for (int i = 0; i < 24; i++) {
        printf("%.1f ", data[i]);
    }
    printf("...\n\n");
    
    // Decomposition
    printf("  %s1. Seasonal Decomposition:%s\n", ASCII_BOLD, ASCII_RESET);
    
    eif_ts_decomposition_t decomp;
    eif_ts_decompose(data, length, season, &decomp, &pool);
    
    printf("    Trend (first 12): ");
    for (int i = 0; i < 12; i++) printf("%.1f ", decomp.trend[i]);
    printf("...\n");
    
    printf("    Season pattern:   ");
    for (int i = 0; i < season; i++) printf("%.2f ", decomp.seasonal[i]);
    printf("\n\n");
    
    // Change Point Detection
    printf("  %s2. Change Point Detection:%s\n", ASCII_BOLD, ASCII_RESET);
    
    // Create data with change point
    float32_t* cp_data = eif_memory_alloc(&pool, 50 * sizeof(float32_t), 4);
    for (int i = 0; i < 25; i++) {
        cp_data[i] = 10.0f + gaussian_noise() * 0.5f;
    }
    for (int i = 25; i < 50; i++) {
        cp_data[i] = 15.0f + gaussian_noise() * 0.5f; // Jump at i=25
    }
    
    int change_points[10];
    int num_changes;
    eif_changepoint_detect(cp_data, 50, 3.0f, change_points, &num_changes, 10);
    
    printf("    Data: [10 ± noise for t<25] → [15 ± noise for t≥25]\n");
    printf("    Detected %d change point(s)", num_changes);
    if (num_changes > 0) {
        printf(" at indices:");
        for (int i = 0; i < num_changes; i++) {
            printf(" %d", change_points[i]);
        }
    }
    printf("\n\n");
    
    // ACF/PACF
    printf("  %s3. Autocorrelation:%s\n", ASCII_BOLD, ASCII_RESET);
    
    float32_t acf[13], pacf[13];
    eif_ts_acf(data, length, acf, 12);
    eif_ts_pacf(data, length, pacf, 12);
    
    printf("    ACF (lags 0-12):  ");
    for (int i = 0; i <= 12; i++) printf("%.2f ", acf[i]);
    printf("\n");
    
    printf("    PACF (lags 0-12): ");
    for (int i = 0; i <= 12; i++) printf("%.2f ", pacf[i]);
    printf("\n\n");
    
    // Correlation
    printf("  %s4. Correlation Analysis:%s\n", ASCII_BOLD, ASCII_RESET);
    
    float32_t x[20], y[20];
    for (int i = 0; i < 20; i++) {
        x[i] = (float32_t)i;
        y[i] = 2.0f * i + 5.0f + gaussian_noise() * 2.0f;
    }
    
    float32_t pearson = eif_correlation_pearson(x, y, 20);
    float32_t spearman = eif_correlation_spearman(x, y, 20, &pool);
    
    printf("    Linear relationship (y ≈ 2x + 5 + noise):\n");
    printf("    Pearson r: %.3f\n", pearson);
    printf("    Spearman ρ: %.3f\n\n", spearman);
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    srand(time(NULL));
    
    printf("\n");
    ascii_section("EIF Tutorial: Phase 1 ML Algorithms");
    
    printf("  This tutorial demonstrates the new machine learning algorithms:\n");
    printf("    1. Isolation Forest (Anomaly Detection)\n");
    printf("    2. Logistic Regression (Classification)\n");
    printf("    3. Decision Tree (Classification/Regression)\n");
    printf("    4. Time Series Extensions\n");
    printf("\n  Press Enter to start...");
    getchar();
    
    // Initialize memory pool
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Run demos
    demo_isolation_forest();
    printf("  Press Enter to continue...");
    getchar();
    
    demo_logistic_regression();
    printf("  Press Enter to continue...");
    getchar();
    
    demo_decision_tree();
    printf("  Press Enter to continue...");
    getchar();
    
    demo_time_series();
    
    // Summary
    printf("\n");
    ascii_section("Tutorial Summary");
    
    printf("  %sKey EIF APIs Demonstrated:%s\n\n", ASCII_BOLD, ASCII_RESET);
    
    printf("  %-35s %s\n", "Isolation Forest:", "Anomaly detection without distribution assumptions");
    printf("    • eif_iforest_init()       - Initialize forest\n");
    printf("    • eif_iforest_fit()        - Train on data\n");
    printf("    • eif_iforest_score()      - Get anomaly score\n\n");
    
    printf("  %-35s %s\n", "Logistic Regression:", "Binary/multiclass classification");
    printf("    • eif_logreg_init()        - Initialize model\n");
    printf("    • eif_logreg_fit()         - Batch training\n");
    printf("    • eif_logreg_update()      - Online SGD update\n\n");
    
    printf("  %-35s %s\n", "Decision Tree:", "Interpretable classification/regression");
    printf("    • eif_dtree_init()         - Initialize tree\n");
    printf("    • eif_dtree_fit()          - Train with CART\n");
    printf("    • eif_dtree_predict()      - Make predictions\n\n");
    
    printf("  %-35s %s\n", "Time Series:", "Extended analysis tools");
    printf("    • eif_ts_decompose()       - Seasonal decomposition\n");
    printf("    • eif_changepoint_detect() - Change point detection\n");
    printf("    • eif_ts_acf/pacf()        - Autocorrelation\n\n");
    
    printf("  %sTutorial Complete!%s\n\n", ASCII_GREEN ASCII_BOLD, ASCII_RESET);
    
    return 0;
}
