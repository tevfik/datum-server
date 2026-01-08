/**
 * @file test_hal_mock.c
 * @brief Unit tests for Mock HAL implementation
 */

#include "../framework/eif_test_runner.h"
#include "eif_hal.h"
#include <string.h>

// Test Timer Functions
bool test_hal_timer(void) {
  uint32_t t1 = eif_hal_timer_ms();
  eif_hal_delay_ms(10);
  uint32_t t2 = eif_hal_timer_ms();

  // Should have elapsed at least 8ms (allow some tolerance)
  uint32_t elapsed = t2 - t1;
  TEST_ASSERT(elapsed >= 8);
  TEST_ASSERT(elapsed < 100);

  return true;
}

// Test GPIO Write/Read
bool test_hal_gpio(void) {
  // Initialize pin as output
  eif_hal_gpio_init(EIF_LED_PIN, EIF_GPIO_OUTPUT);

  // Write should not crash
  eif_hal_gpio_write(EIF_LED_PIN, 1);
  eif_hal_gpio_write(EIF_LED_PIN, 0);
  eif_hal_gpio_toggle(EIF_LED_PIN);

  // For mock, read always returns 0 (no state tracking)
  int val = eif_hal_gpio_read(EIF_LED_PIN);
  TEST_ASSERT(val == 0 || val == 1);

  return true;
}

// Test IMU Read
bool test_hal_imu(void) {
  // Initialize IMU first
  eif_imu_config_t imu_config = {.sample_rate_hz = 100.0f};
  int init_ret = eif_hal_imu_init(&imu_config);
  TEST_ASSERT_EQUAL_INT(0, init_ret);

  eif_imu_data_t data = {0};
  int ret = eif_hal_imu_read(&data);
  TEST_ASSERT_EQUAL_INT(0, ret);

  // Check that values are reasonable (not NaN, not huge)
  TEST_ASSERT(isfinite(data.ax));
  TEST_ASSERT(isfinite(data.ay));
  TEST_ASSERT(isfinite(data.az));
  TEST_ASSERT(isfinite(data.gx));
  TEST_ASSERT(isfinite(data.gy));
  TEST_ASSERT(isfinite(data.gz));

  // az should be positive (has gravity component)
  TEST_ASSERT(data.az > 0.5f);

  return true;
}

// Test Platform Name
bool test_hal_platform(void) {
  const char *name = eif_hal_get_platform_name();
  TEST_ASSERT_NOT_NULL(name);
  TEST_ASSERT(strlen(name) > 0);

  return true;
}

BEGIN_TEST_SUITE(run_hal_mock_tests)
RUN_TEST(test_hal_timer);
RUN_TEST(test_hal_gpio);
RUN_TEST(test_hal_imu);
RUN_TEST(test_hal_platform);
END_TEST_SUITE()
