/**
 * @file eif_hal_mock_generic.c
 * @brief Generic Mock HAL Implementation
 */

#include "eif_hal.h"
#include "eif_hal_mock.h"
#include <math.h>
#include <stdio.h>
#include <unistd.h>

// Init and Timing are handled by platforms/generic/eif_hal_generic.c

// GPIO Simulation
void eif_hal_gpio_write(int pin, int value) {
  const char *pin_name = "UNKNOWN";
  if (pin == EIF_GPIO_PIN_LED_RED)
    pin_name = "LED_RED";
  if (pin == EIF_GPIO_PIN_LED_GREEN)
    pin_name = "LED_GREEN";

  // printf("[GPIO] %s -> %s\n", pin_name, value ? "HIGH" : "LOW");
}

void eif_hal_gpio_toggle(int pin) {
  // State tracking not implemented in simple mock
  eif_hal_gpio_write(pin, 1);
}

int eif_hal_gpio_read(int pin) {
  // Simulate User Button press?
  return 0;
}

// IMU Simulation
int eif_hal_imu_read(eif_imu_data_t *data) {
  if (!data)
    return -1;

  // Generate synthetic motion: Static + Slight Noise + Gravity on Z
  uint32_t t = eif_hal_timer_ms();
  float time_s = t / 1000.0f;

  // Example: Rotate around Z axis slowly
  data->gx = 0.0f;
  data->gy = 0.0f;
  data->gz = 0.5f * sinf(time_s); // Oscillating rotation

  // Gravity + Noise on Accel
  data->ax = 0.1f * sinf(time_s * 2);
  data->ay = 0.1f * cosf(time_s * 2);
  data->az = 9.81f; // Gravity

  return 0; // Success
}
