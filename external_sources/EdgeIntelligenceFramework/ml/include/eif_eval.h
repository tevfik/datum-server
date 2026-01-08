/**
 * @file eif_eval.h
 * @brief Onboard Evaluation Tools for EIF
 *
 * Provides evaluation metrics for model validation on MCU.
 * Similar to nnom's onboard evaluation capabilities.
 *
 * Features:
 * - Accuracy measurement
 * - Confusion matrix computation
 * - Loss calculation (cross-entropy, MSE)
 * - Per-class metrics (precision, recall, F1)
 * - Running statistics (incremental updates)
 *
 * Example:
 *   eif_eval_t eval;
 *   eif_eval_init(&eval, 10);  // 10 classes
 *
 *   for (int i = 0; i < test_size; i++) {
 *       model_infer(test_x[i], pred);
 *       eif_eval_update(&eval, pred, test_y[i]);
 *   }
 *
 *   eif_eval_print(&eval);
 */

#ifndef EIF_EVAL_H
#define EIF_EVAL_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Configuration
// =============================================================================

#ifndef EIF_EVAL_MAX_CLASSES
#define EIF_EVAL_MAX_CLASSES 32
#endif

// =============================================================================
// Evaluation Metrics Structure
// =============================================================================

/**
 * @brief Evaluation metrics state
 */
typedef struct {
  // Configuration
  int num_classes;

  // Confusion matrix [predicted][actual]
  uint32_t confusion[EIF_EVAL_MAX_CLASSES][EIF_EVAL_MAX_CLASSES];

  // Running totals
  uint32_t total_samples;
  uint32_t correct_samples;

  // Per-class counts
  uint32_t true_positives[EIF_EVAL_MAX_CLASSES];
  uint32_t false_positives[EIF_EVAL_MAX_CLASSES];
  uint32_t false_negatives[EIF_EVAL_MAX_CLASSES];

  // Loss accumulator (Q15 fixed-point)
  int64_t total_loss;

  // Timing (optional)
  uint32_t total_inference_us;
  uint32_t max_inference_us;
  uint32_t min_inference_us;
} eif_eval_t;

// =============================================================================
// API Functions
// =============================================================================

/**
 * @brief Initialize evaluation context
 * @param eval Evaluation context
 * @param num_classes Number of output classes
 */
static inline void eif_eval_init(eif_eval_t *eval, int num_classes) {
  memset(eval, 0, sizeof(eif_eval_t));
  eval->num_classes =
      (num_classes > EIF_EVAL_MAX_CLASSES) ? EIF_EVAL_MAX_CLASSES : num_classes;
  eval->min_inference_us = 0xFFFFFFFF;
}

/**
 * @brief Reset evaluation statistics
 */
static inline void eif_eval_reset(eif_eval_t *eval) {
  int nc = eval->num_classes;
  eif_eval_init(eval, nc);
}

/**
 * @brief Find argmax of Q15 output vector
 */
static inline int eif_eval_argmax(const int16_t *output, int size) {
  int max_idx = 0;
  int16_t max_val = output[0];
  for (int i = 1; i < size; i++) {
    if (output[i] > max_val) {
      max_val = output[i];
      max_idx = i;
    }
  }
  return max_idx;
}

/**
 * @brief Update evaluation with single sample (classification)
 * @param eval Evaluation context
 * @param output Model output (Q15 probabilities)
 * @param true_label Ground truth class index
 */
static inline void eif_eval_update(eif_eval_t *eval, const int16_t *output,
                                   int true_label) {
  int predicted = eif_eval_argmax(output, eval->num_classes);

  eval->total_samples++;
  eval->confusion[predicted][true_label]++;

  if (predicted == true_label) {
    eval->correct_samples++;
    eval->true_positives[predicted]++;
  } else {
    eval->false_positives[predicted]++;
    eval->false_negatives[true_label]++;
  }

  // Cross-entropy loss: -log(p[true_label])
  // Approximate in fixed-point
  int16_t p_true = output[true_label];
  if (p_true <= 0)
    p_true = 1; // Avoid log(0)

  // Simple log approximation using leading zeros
  int32_t log_approx = 0;
  int32_t x = p_true;
  while (x < 16384 && log_approx < 15) {
    x <<= 1;
    log_approx++;
  }
  eval->total_loss += log_approx * 2184; // Scale factor for ~ln(2)
}

/**
 * @brief Update evaluation with timing info
 */
static inline void eif_eval_update_timing(eif_eval_t *eval,
                                          uint32_t inference_us) {
  eval->total_inference_us += inference_us;

  if (inference_us > eval->max_inference_us) {
    eval->max_inference_us = inference_us;
  }
  if (inference_us < eval->min_inference_us) {
    eval->min_inference_us = inference_us;
  }
}

// =============================================================================
// Metric Calculations
// =============================================================================

/**
 * @brief Get overall accuracy (0.0 to 1.0)
 */
static inline float eif_eval_accuracy(const eif_eval_t *eval) {
  if (eval->total_samples == 0)
    return 0.0f;
  return (float)eval->correct_samples / (float)eval->total_samples;
}

/**
 * @brief Get per-class precision
 */
static inline float eif_eval_precision(const eif_eval_t *eval, int class_idx) {
  uint32_t tp = eval->true_positives[class_idx];
  uint32_t fp = eval->false_positives[class_idx];
  if (tp + fp == 0)
    return 0.0f;
  return (float)tp / (float)(tp + fp);
}

/**
 * @brief Get per-class recall (sensitivity)
 */
static inline float eif_eval_recall(const eif_eval_t *eval, int class_idx) {
  uint32_t tp = eval->true_positives[class_idx];
  uint32_t fn = eval->false_negatives[class_idx];
  if (tp + fn == 0)
    return 0.0f;
  return (float)tp / (float)(tp + fn);
}

/**
 * @brief Get per-class F1 score
 */
static inline float eif_eval_f1(const eif_eval_t *eval, int class_idx) {
  float p = eif_eval_precision(eval, class_idx);
  float r = eif_eval_recall(eval, class_idx);
  if (p + r == 0)
    return 0.0f;
  return 2.0f * p * r / (p + r);
}

/**
 * @brief Get macro F1 score (average of per-class F1)
 */
static inline float eif_eval_macro_f1(const eif_eval_t *eval) {
  float sum = 0.0f;
  int count = 0;
  for (int i = 0; i < eval->num_classes; i++) {
    float f1 = eif_eval_f1(eval, i);
    if (f1 > 0.0f) {
      sum += f1;
      count++;
    }
  }
  return count > 0 ? sum / count : 0.0f;
}

/**
 * @brief Get average cross-entropy loss
 */
static inline float eif_eval_loss(const eif_eval_t *eval) {
  if (eval->total_samples == 0)
    return 0.0f;
  return (float)eval->total_loss / (float)eval->total_samples / 32767.0f;
}

/**
 * @brief Get average inference time in microseconds
 */
static inline float eif_eval_avg_time_us(const eif_eval_t *eval) {
  if (eval->total_samples == 0)
    return 0.0f;
  return (float)eval->total_inference_us / (float)eval->total_samples;
}

// =============================================================================
// Printing (requires printf)
// =============================================================================

#ifdef EIF_HAS_PRINTF
#include <stdio.h>

/**
 * @brief Print evaluation summary
 */
static inline void eif_eval_print_summary(const eif_eval_t *eval) {
  printf("\n╔═══════════════════════════════════════════════════════╗\n");
  printf("║              Model Evaluation Results                   ║\n");
  printf("╚═══════════════════════════════════════════════════════╝\n\n");

  printf("Overall Metrics:\n");
  printf("───────────────────────────────────────────────────────\n");
  printf("  Samples:    %lu\n", (unsigned long)eval->total_samples);
  printf("  Correct:    %lu\n", (unsigned long)eval->correct_samples);
  printf("  Accuracy:   %.2f%%\n", eif_eval_accuracy(eval) * 100.0f);
  printf("  Macro F1:   %.4f\n", eif_eval_macro_f1(eval));
  printf("  Avg Loss:   %.4f\n", eif_eval_loss(eval));

  if (eval->total_inference_us > 0) {
    printf("\nTiming:\n");
    printf("───────────────────────────────────────────────────────\n");
    printf("  Avg:        %.2f µs\n", eif_eval_avg_time_us(eval));
    printf("  Min:        %lu µs\n", (unsigned long)eval->min_inference_us);
    printf("  Max:        %lu µs\n", (unsigned long)eval->max_inference_us);
  }
}

/**
 * @brief Print per-class metrics
 */
static inline void eif_eval_print_classes(const eif_eval_t *eval) {
  printf("\nPer-Class Metrics:\n");
  printf("───────────────────────────────────────────────────────\n");
  printf("  Class   Precision   Recall      F1     Support\n");
  printf("───────────────────────────────────────────────────────\n");

  for (int i = 0; i < eval->num_classes; i++) {
    uint32_t support = eval->true_positives[i] + eval->false_negatives[i];
    if (support > 0) {
      printf("  %3d      %6.2f%%   %6.2f%%   %6.4f   %5lu\n", i,
             eif_eval_precision(eval, i) * 100.0f,
             eif_eval_recall(eval, i) * 100.0f, eif_eval_f1(eval, i),
             (unsigned long)support);
    }
  }
}

/**
 * @brief Print confusion matrix
 */
static inline void eif_eval_print_confusion(const eif_eval_t *eval) {
  printf("\nConfusion Matrix:\n");
  printf("───────────────────────────────────────────────────────\n");

  // Header row
  printf("       ");
  for (int j = 0; j < eval->num_classes && j < 10; j++) {
    printf("%5d", j);
  }
  printf("  <- Actual\n");

  // Data rows
  for (int i = 0; i < eval->num_classes && i < 10; i++) {
    printf("  %3d: ", i);
    for (int j = 0; j < eval->num_classes && j < 10; j++) {
      uint32_t count = eval->confusion[i][j];
      if (count > 0) {
        printf("%5lu", (unsigned long)count);
      } else {
        printf("    .");
      }
    }
    printf("\n");
  }
  printf("   ^\n   Predicted\n");
}

/**
 * @brief Print full evaluation report
 */
static inline void eif_eval_print(const eif_eval_t *eval) {
  eif_eval_print_summary(eval);
  eif_eval_print_classes(eval);
  eif_eval_print_confusion(eval);
}

#endif // EIF_HAS_PRINTF

// =============================================================================
// Profiler for Layer-by-Layer Analysis
// =============================================================================

#ifndef EIF_PROFILE_MAX_LAYERS
#define EIF_PROFILE_MAX_LAYERS 32
#endif

typedef struct {
  const char *name;
  uint32_t total_us;
  uint32_t call_count;
  uint32_t min_us;
  uint32_t max_us;
} eif_layer_profile_t;

typedef struct {
  eif_layer_profile_t layers[EIF_PROFILE_MAX_LAYERS];
  int num_layers;
  uint32_t total_inference_us;
  uint32_t num_inferences;

  // Memory tracking
  size_t peak_memory;
  size_t current_memory;
} eif_profiler_t;

/**
 * @brief Initialize profiler
 */
static inline void eif_profiler_init(eif_profiler_t *prof) {
  memset(prof, 0, sizeof(eif_profiler_t));
  for (int i = 0; i < EIF_PROFILE_MAX_LAYERS; i++) {
    prof->layers[i].min_us = 0xFFFFFFFF;
  }
}

/**
 * @brief Add a layer to profile
 */
static inline int eif_profiler_add_layer(eif_profiler_t *prof,
                                         const char *name) {
  if (prof->num_layers >= EIF_PROFILE_MAX_LAYERS)
    return -1;
  prof->layers[prof->num_layers].name = name;
  return prof->num_layers++;
}

/**
 * @brief Record layer execution time
 */
static inline void eif_profiler_record(eif_profiler_t *prof, int layer_idx,
                                       uint32_t time_us) {
  if (layer_idx < 0 || layer_idx >= prof->num_layers)
    return;

  eif_layer_profile_t *lp = &prof->layers[layer_idx];
  lp->total_us += time_us;
  lp->call_count++;

  if (time_us < lp->min_us)
    lp->min_us = time_us;
  if (time_us > lp->max_us)
    lp->max_us = time_us;
}

/**
 * @brief Record total inference time
 */
static inline void eif_profiler_inference(eif_profiler_t *prof,
                                          uint32_t time_us) {
  prof->total_inference_us += time_us;
  prof->num_inferences++;
}

/**
 * @brief Update memory usage
 */
static inline void eif_profiler_memory(eif_profiler_t *prof, size_t current) {
  prof->current_memory = current;
  if (current > prof->peak_memory) {
    prof->peak_memory = current;
  }
}

#ifdef EIF_HAS_PRINTF

/**
 * @brief Print profiler results
 */
static inline void eif_profiler_print(const eif_profiler_t *prof) {
  printf("\n╔═══════════════════════════════════════════════════════╗\n");
  printf("║              Layer-by-Layer Profiling                   ║\n");
  printf("╚═══════════════════════════════════════════════════════╝\n\n");

  if (prof->num_inferences > 0) {
    float avg_total = (float)prof->total_inference_us / prof->num_inferences;
    printf("Total inference: %.2f µs avg (%lu calls)\n\n", avg_total,
           (unsigned long)prof->num_inferences);
  }

  printf("%-20s %10s %10s %10s %8s\n", "Layer", "Avg (µs)", "Min (µs)",
         "Max (µs)", "Calls");
  printf("───────────────────────────────────────────────────────────\n");

  for (int i = 0; i < prof->num_layers; i++) {
    const eif_layer_profile_t *lp = &prof->layers[i];
    if (lp->call_count > 0) {
      float avg = (float)lp->total_us / lp->call_count;
      printf("%-20s %10.2f %10lu %10lu %8lu\n",
             lp->name ? lp->name : "(unnamed)", avg, (unsigned long)lp->min_us,
             (unsigned long)lp->max_us, (unsigned long)lp->call_count);
    }
  }

  printf("\nMemory:\n");
  printf("───────────────────────────────────────────────────────\n");
  printf("  Peak:    %lu bytes\n", (unsigned long)prof->peak_memory);
  printf("  Current: %lu bytes\n", (unsigned long)prof->current_memory);
}

#endif // EIF_HAS_PRINTF

#ifdef __cplusplus
}
#endif

#endif // EIF_EVAL_H
