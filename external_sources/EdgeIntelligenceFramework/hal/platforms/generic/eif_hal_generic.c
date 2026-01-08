/**
 * @file eif_hal_generic.c
 * @brief Generic (POSIX/Linux) HAL Implementation
 */

#include "eif_hal.h"

#ifdef EIF_HAL_PLATFORM_POSIX

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/sysinfo.h>
#endif

static struct timespec start_time;
static bool timer_initialized = false;

// =============================================================================
// Timing
// =============================================================================

int eif_hal_timer_init(void) {
  clock_gettime(CLOCK_MONOTONIC, &start_time);
  timer_initialized = true;
  return 0;
}

uint64_t eif_hal_get_time_us(void) {
  if (!timer_initialized) {
    eif_hal_timer_init();
  }

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  uint64_t elapsed_sec = now.tv_sec - start_time.tv_sec;
  int64_t elapsed_nsec = now.tv_nsec - start_time.tv_nsec;

  return elapsed_sec * 1000000ULL + (elapsed_nsec / 1000);
}

uint32_t eif_hal_get_time_ms(void) {
  return (uint32_t)(eif_hal_get_time_us() / 1000);
}

// Aliases for mock HAL compatibility
uint32_t eif_hal_timer_ms(void) { return eif_hal_get_time_ms(); }

uint32_t eif_hal_timer_us(void) { return (uint32_t)eif_hal_get_time_us(); }

void eif_hal_delay_ms(uint32_t ms) { usleep(ms * 1000); }

void eif_hal_delay_us(uint32_t us) { usleep(us); }

// =============================================================================
// Memory
// =============================================================================

size_t eif_hal_heap_free(void) {
#ifdef __linux__
  struct sysinfo info;
  if (sysinfo(&info) == 0) {
    return info.freeram;
  }
#endif
  return 0; // Unknown
}

size_t eif_hal_heap_total(void) {
#ifdef __linux__
  struct sysinfo info;
  if (sysinfo(&info) == 0) {
    return info.totalram;
  }
#endif
  return 0;
}

void *eif_hal_aligned_alloc(size_t size, size_t alignment) {
  void *ptr = NULL;
  if (posix_memalign(&ptr, alignment, size) == 0) {
    return ptr;
  }
  return NULL;
}

void eif_hal_aligned_free(void *ptr) { free(ptr); }

// =============================================================================
// Critical Sections (no-op on POSIX)
// =============================================================================

uint32_t eif_hal_critical_enter(void) {
  return 0; // Not applicable on POSIX
}

void eif_hal_critical_exit(uint32_t state) { (void)state; }

// =============================================================================
// Debug
// =============================================================================

void eif_hal_debug_print(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

void eif_hal_assert(bool condition, const char *msg) {
  if (!condition) {
    fprintf(stderr, "ASSERT FAILED: %s\n", msg);
    abort();
  }
}

// =============================================================================
// Hardware Info
// =============================================================================

uint32_t eif_hal_get_cpu_freq(void) {
#ifdef __linux__
  FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
  if (f) {
    uint32_t freq_khz = 0;
    if (fscanf(f, "%u", &freq_khz) == 1) {
      fclose(f);
      return freq_khz * 1000; // Convert to Hz
    }
    fclose(f);
  }
#endif
  return 1000000000; // Default 1 GHz
}

const char *eif_hal_get_platform(void) {
#if defined(__linux__)
  return "Linux";
#elif defined(__APPLE__)
  return "macOS";
#else
  return "POSIX";
#endif
}

bool eif_hal_has_simd(void) {
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  return true;
#elif defined(__SSE2__)
  return true;
#else
  return false;
#endif
}

bool eif_hal_has_fpu(void) {
  return true; // Modern POSIX systems have FPU
}

// =============================================================================
// Cycle Counter (using clock)
// =============================================================================

static uint64_t cycle_start = 0;

void eif_hal_cycle_counter_start(void) { cycle_start = eif_hal_get_time_us(); }

uint32_t eif_hal_cycle_counter_read(void) {
  uint64_t elapsed = eif_hal_get_time_us() - cycle_start;
  // Approximate cycles (assuming 1 GHz)
  return (uint32_t)(elapsed * 1000);
}

void eif_hal_cycle_counter_stop(void) { cycle_start = 0; }

// =============================================================================
// Power
// =============================================================================

void eif_hal_sleep(void) {
  usleep(1000); // 1ms sleep
}

void eif_hal_deep_sleep(void) {
  sleep(1); // 1s sleep
}

#endif // EIF_HAL_PLATFORM_POSIX
