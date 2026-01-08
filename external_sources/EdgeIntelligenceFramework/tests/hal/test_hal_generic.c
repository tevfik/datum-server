/**
 * @file test_hal_generic.c
 * @brief Unit tests for Generic HAL implementation
 */

#include "../framework/eif_test_runner.h"
#include "eif_hal.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Declarations for functions in eif_hal_generic.c that are not in eif_hal.h
size_t eif_hal_heap_free(void);
size_t eif_hal_heap_total(void);
void *eif_hal_aligned_alloc(size_t size, size_t alignment);
void eif_hal_aligned_free(void *ptr);
uint32_t eif_hal_critical_enter(void);
void eif_hal_critical_exit(uint32_t state);
void eif_hal_debug_print(const char *fmt, ...);
void eif_hal_assert(bool condition, const char *msg);
uint32_t eif_hal_get_cpu_freq(void);
const char *eif_hal_get_platform(void);
bool eif_hal_has_simd(void);
bool eif_hal_has_fpu(void);
void eif_hal_cycle_counter_start(void);
uint32_t eif_hal_cycle_counter_read(void);
void eif_hal_cycle_counter_stop(void);
void eif_hal_sleep(void);
void eif_hal_deep_sleep(void);

// Test Memory Functions
bool test_hal_memory(void) {
  // Test aligned allocation
  size_t size = 1024;
  size_t alignment = 64;
  void *ptr = eif_hal_aligned_alloc(size, alignment);
  
  TEST_ASSERT(ptr != NULL);
  TEST_ASSERT(((uintptr_t)ptr % alignment) == 0);
  
  // Write to memory to ensure it's valid
  memset(ptr, 0xAA, size);
  
  eif_hal_aligned_free(ptr);
  
  // Test heap info (might return 0 on some platforms, but shouldn't crash)
  size_t total = eif_hal_heap_total();
  size_t free_mem = eif_hal_heap_free();
  
  // Just verify they don't crash and return reasonable values (or 0)
  (void)total;
  (void)free_mem;

  return true;
}

// Test Hardware Info
bool test_hal_hw_info(void) {
  uint32_t freq = eif_hal_get_cpu_freq();
  TEST_ASSERT(freq > 0);
  
  const char *platform = eif_hal_get_platform();
  TEST_ASSERT(platform != NULL);
  TEST_ASSERT(strlen(platform) > 0);
  
  bool has_simd = eif_hal_has_simd();
  bool has_fpu = eif_hal_has_fpu();
  
  // Just ensure they return boolean values
  (void)has_simd;
  (void)has_fpu;
  
  return true;
}

// Test Cycle Counter
bool test_hal_cycle_counter(void) {
  eif_hal_cycle_counter_start();
  
  // Do some work
  volatile int x = 0;
  for(int i=0; i<1000; i++) {
    x++;
  }
  
  uint32_t cycles = eif_hal_cycle_counter_read();
  TEST_ASSERT(cycles > 0);
  
  eif_hal_cycle_counter_stop();
  
  return true;
}

// Test Debug Functions
bool test_hal_debug(void) {
  // Just ensure it doesn't crash
  eif_hal_debug_print("Test debug print: %d\n", 123);
  
  // Test assert (true condition)
  eif_hal_assert(true, "Should not fail");
  
  return true;
}

// Test Critical Section (No-op on POSIX but should be callable)
bool test_hal_critical(void) {
  uint32_t state = eif_hal_critical_enter();
  eif_hal_critical_exit(state);
  return true;
}

// Test Sleep
bool test_hal_sleep_funcs(void) {
    // Just test they don't crash. 
    // We don't want to sleep for too long in unit tests.
    // eif_hal_sleep() sleeps for 1ms
    eif_hal_sleep();
    
    // eif_hal_deep_sleep() sleeps for 1s - might be too long for unit tests?
    // Let's skip deep_sleep to avoid slowing down tests too much, 
    // or maybe just call it once.
    // eif_hal_deep_sleep(); 
    
    return true;
}

int run_hal_generic_tests(void) {
  int failed = 0;
  
  RUN_TEST(test_hal_memory);
  RUN_TEST(test_hal_hw_info);
  RUN_TEST(test_hal_cycle_counter);
  RUN_TEST(test_hal_debug);
  RUN_TEST(test_hal_critical);
  RUN_TEST(test_hal_sleep_funcs);
  
  return failed;
}
