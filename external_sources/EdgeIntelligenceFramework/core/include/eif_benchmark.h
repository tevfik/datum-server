/**
 * @file eif_benchmark.h
 * @brief Benchmarking Utilities for Edge Intelligence
 *
 * Portable benchmarking tools for:
 * - Timing measurements
 * - Memory profiling
 * - Throughput calculation
 * - Statistical analysis
 *
 * Works on embedded systems with minimal overhead.
 */

#ifndef EIF_BENCHMARK_H
#define EIF_BENCHMARK_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Platform-specific timing
// =============================================================================

#if defined(_WIN32)
#include <windows.h>
typedef LARGE_INTEGER eif_time_t;

static inline void eif_time_get(eif_time_t *t) { QueryPerformanceCounter(t); }

static inline double eif_time_diff_us(eif_time_t *start, eif_time_t *end) {
  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);
  return (double)(end->QuadPart - start->QuadPart) * 1000000.0 / freq.QuadPart;
}
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/time.h>
typedef struct timeval eif_time_t;

static inline void eif_time_get(eif_time_t *t) { gettimeofday(t, NULL); }

static inline double eif_time_diff_us(eif_time_t *start, eif_time_t *end) {
  return (end->tv_sec - start->tv_sec) * 1000000.0 +
         (end->tv_usec - start->tv_usec);
}
#else
// Generic fallback (needs platform timer)
typedef uint32_t eif_time_t;

static inline void eif_time_get(eif_time_t *t) {
  *t = 0; // Platform-specific implementation needed
}

static inline double eif_time_diff_us(eif_time_t *start, eif_time_t *end) {
  return (double)(*end - *start);
}
#endif

// =============================================================================
// Benchmark Result
// =============================================================================

/**
 * @brief Benchmark result statistics
 */
typedef struct {
  char name[64];
  int iterations;
  double total_us;
  double min_us;
  double max_us;
  double mean_us;
  double std_us;
  double ops_per_sec;
  size_t memory_bytes;
} eif_benchmark_result_t;

/**
 * @brief Initialize benchmark result
 */
static inline void eif_benchmark_result_init(eif_benchmark_result_t *r,
                                             const char *name) {
  strncpy(r->name, name, 63);
  r->name[63] = '\0';
  r->iterations = 0;
  r->total_us = 0;
  r->min_us = 1e30;
  r->max_us = 0;
  r->mean_us = 0;
  r->std_us = 0;
  r->ops_per_sec = 0;
  r->memory_bytes = 0;
}

// =============================================================================
// Benchmark Runner
// =============================================================================

/**
 * @brief Benchmark context for running tests
 */
typedef struct {
  eif_time_t start;
  eif_time_t end;
  double times[1000];
  int num_runs;
  int warmup_runs;
} eif_benchmark_t;

/**
 * @brief Initialize benchmark
 */
static inline void eif_benchmark_init(eif_benchmark_t *b, int warmup) {
  b->num_runs = 0;
  b->warmup_runs = warmup;
}

/**
 * @brief Start timing
 */
static inline void eif_benchmark_start(eif_benchmark_t *b) {
  eif_time_get(&b->start);
}

/**
 * @brief Stop timing and record
 */
static inline void eif_benchmark_stop(eif_benchmark_t *b) {
  eif_time_get(&b->end);

  if (b->warmup_runs > 0) {
    b->warmup_runs--;
    return;
  }

  if (b->num_runs < 1000) {
    b->times[b->num_runs] = eif_time_diff_us(&b->start, &b->end);
    b->num_runs++;
  }
}

/**
 * @brief Calculate statistics
 */
static inline void eif_benchmark_stats(eif_benchmark_t *b,
                                       eif_benchmark_result_t *r) {
  if (b->num_runs == 0)
    return;

  r->iterations = b->num_runs;

  // Calculate statistics
  double sum = 0;
  r->min_us = 1e30;
  r->max_us = 0;

  for (int i = 0; i < b->num_runs; i++) {
    sum += b->times[i];
    if (b->times[i] < r->min_us)
      r->min_us = b->times[i];
    if (b->times[i] > r->max_us)
      r->max_us = b->times[i];
  }

  r->mean_us = sum / b->num_runs;
  r->total_us = sum;

  // Standard deviation
  double var_sum = 0;
  for (int i = 0; i < b->num_runs; i++) {
    double diff = b->times[i] - r->mean_us;
    var_sum += diff * diff;
  }
  r->std_us = sqrt(var_sum / b->num_runs);

  // Operations per second
  r->ops_per_sec = 1000000.0 / r->mean_us;
}

/**
 * @brief Print benchmark result
 */
static inline void eif_benchmark_print(eif_benchmark_result_t *r) {
  printf("Benchmark: %s\n", r->name);
  printf("  Iterations: %d\n", r->iterations);
  printf("  Mean:       %.2f µs\n", r->mean_us);
  printf("  Std Dev:    %.2f µs\n", r->std_us);
  printf("  Min:        %.2f µs\n", r->min_us);
  printf("  Max:        %.2f µs\n", r->max_us);
  printf("  Throughput: %.0f ops/sec\n", r->ops_per_sec);
  if (r->memory_bytes > 0) {
    printf("  Memory:     %zu bytes\n", r->memory_bytes);
  }
  printf("\n");
}

/**
 * @brief Print compact benchmark table row
 */
static inline void eif_benchmark_print_row(eif_benchmark_result_t *r) {
  printf("| %-30s | %8.1f µs | %8.0f ops/s | %6zu B |\n", r->name, r->mean_us,
         r->ops_per_sec, r->memory_bytes);
}

/**
 * @brief Print benchmark table header
 */
static inline void eif_benchmark_print_header(void) {
  printf("| %-30s | %11s | %13s | %8s |\n", "Benchmark", "Time", "Throughput",
         "Memory");
  printf("|%s|%s|%s|%s|\n", "--------------------------------", "-------------",
         "---------------", "----------");
}

// =============================================================================
// Memory Profiling
// =============================================================================

/**
 * @brief Estimate struct memory usage
 */
#define EIF_SIZEOF(type) printf("sizeof(" #type ") = %zu bytes\n", sizeof(type))

/**
 * @brief Memory usage summary
 */
typedef struct {
  size_t stack_bytes;
  size_t heap_bytes;
  size_t flash_bytes; // Constant data
  size_t total_bytes;
} eif_memory_usage_t;

/**
 * @brief Print memory usage
 */
static inline void eif_memory_print(eif_memory_usage_t *m, const char *name) {
  printf("Memory: %s\n", name);
  printf("  Stack:  %zu bytes\n", m->stack_bytes);
  printf("  Heap:   %zu bytes\n", m->heap_bytes);
  printf("  Flash:  %zu bytes\n", m->flash_bytes);
  printf("  Total:  %zu bytes (%.1f KB)\n", m->total_bytes,
         m->total_bytes / 1024.0);
}

// =============================================================================
// Comparison Utilities
// =============================================================================

/**
 * @brief Compare two benchmark results
 */
static inline void eif_benchmark_compare(eif_benchmark_result_t *a,
                                         eif_benchmark_result_t *b) {
  double speedup = b->mean_us / a->mean_us;
  printf("Comparison: %s vs %s\n", a->name, b->name);
  printf("  %s: %.2f µs\n", a->name, a->mean_us);
  printf("  %s: %.2f µs\n", b->name, b->mean_us);
  printf("  Speedup: %.2fx (%s is faster)\n",
         speedup > 1 ? speedup : 1 / speedup, speedup > 1 ? a->name : b->name);
}

#ifdef __cplusplus
}
#endif

#endif // EIF_BENCHMARK_H
