/**
 * @file eif_hal_impl_mock.c
 * @brief Mock HAL implementation for PC-based testing
 *
 * Implements the eif_hal.h interface using synthetic data.
 * Compile with: -DEIF_USE_MOCK_HAL
 */

#include "eif_hal.h"
#include "eif_hal_mock.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Internal State
// ============================================================================

static struct {
  bool initialized;
  float sample_rate_hz;
  uint32_t sample_count;
  float accel_bias[3];
  float gyro_bias[3];
  eif_mock_imu_config_t config;
} imu_state = {0};

static struct {
  bool initialized;
  int sample_rate_hz;
  uint32_t sample_count;
  eif_mock_audio_config_t config;
} audio_state = {0};

static struct {
  int mode[64];  // GPIO modes
  int value[64]; // GPIO values
} gpio_state = {0};

static struct timespec start_time;
static bool timer_initialized = false;

// ============================================================================
// Timer Implementation (only if POSIX HAL not present)
// ============================================================================

#ifndef EIF_HAL_PLATFORM_POSIX

static void ensure_timer_init(void) {
  if (!timer_initialized) {
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    timer_initialized = true;
  }
}

uint32_t eif_hal_timer_us(void) {
  ensure_timer_init();
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return ((now.tv_sec - start_time.tv_sec) * 1000000) +
         ((now.tv_nsec - start_time.tv_nsec) / 1000);
}

uint32_t eif_hal_timer_ms(void) { return eif_hal_timer_us() / 1000; }

void eif_hal_delay_us(uint32_t us) {
  struct timespec ts = {.tv_sec = us / 1000000,
                        .tv_nsec = (us % 1000000) * 1000};
  nanosleep(&ts, NULL);
}

void eif_hal_delay_ms(uint32_t ms) { eif_hal_delay_us(ms * 1000); }

#endif // !EIF_HAL_PLATFORM_POSIX

// ============================================================================
// IMU Implementation
// ============================================================================

int eif_hal_imu_init(const eif_imu_config_t *config) {
  if (!config)
    return -1;

  imu_state.sample_rate_hz =
      config->sample_rate_hz > 0 ? config->sample_rate_hz : 100.0f;
  imu_state.sample_count = 0;

  // Initialize mock IMU config
  imu_state.config.mode = EIF_MOCK_IMU_SINE;
  imu_state.config.sample_rate = imu_state.sample_rate_hz;
  imu_state.config.noise_level = 0.01f;
  imu_state.config.frequency = 0.5f;
  imu_state.config.amplitude = 0.3f;

  // Random biases for realism
  srand(time(NULL));
  for (int i = 0; i < 3; i++) {
    imu_state.accel_bias[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    imu_state.gyro_bias[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.02f;
  }

  imu_state.initialized = true;
  return 0;
}

int eif_hal_imu_read(eif_imu_data_t *data) {
  if (!imu_state.initialized || !data)
    return -1;

  float t = (float)imu_state.sample_count / imu_state.sample_rate_hz;
  imu_state.sample_count++;

  // Generate based on mode
  float noise = imu_state.config.noise_level;
  float freq = imu_state.config.frequency;
  float amp = imu_state.config.amplitude;

  // Synthetic accelerometer (gravity + motion)
  data->ax = amp * sinf(2 * M_PI * freq * t) + imu_state.accel_bias[0] +
             (float)(rand() % 1000 - 500) / 500000.0f * noise;
  data->ay = amp * cosf(2 * M_PI * freq * 1.1f * t) + imu_state.accel_bias[1] +
             (float)(rand() % 1000 - 500) / 500000.0f * noise;
  data->az = 1.0f + 0.1f * sinf(2 * M_PI * freq * 0.5f * t) +
             imu_state.accel_bias[2] +
             (float)(rand() % 1000 - 500) / 500000.0f * noise; // Gravity

  // Synthetic gyroscope
  data->gx = 0.5f * sinf(2 * M_PI * freq * 0.8f * t) + imu_state.gyro_bias[0] +
             (float)(rand() % 1000 - 500) / 50000.0f * noise;
  data->gy = 0.3f * cosf(2 * M_PI * freq * 0.7f * t) + imu_state.gyro_bias[1] +
             (float)(rand() % 1000 - 500) / 50000.0f * noise;
  data->gz = 0.1f * sinf(2 * M_PI * freq * 0.3f * t) + imu_state.gyro_bias[2] +
             (float)(rand() % 1000 - 500) / 50000.0f * noise;

  // No magnetometer in mock
  data->mx = data->my = data->mz = 0.0f;

  data->timestamp = eif_hal_timer_us();

  return 0;
}

bool eif_hal_imu_data_ready(void) { return imu_state.initialized; }

int eif_hal_imu_calibrate(void) {
  if (!imu_state.initialized)
    return -1;

  printf("[Mock HAL] Calibrating IMU...\n");
  eif_hal_delay_ms(500);

  // Reset biases in calibration
  for (int i = 0; i < 3; i++) {
    imu_state.accel_bias[i] = 0.0f;
    imu_state.gyro_bias[i] = 0.0f;
  }

  printf("[Mock HAL] IMU calibration complete\n");
  return 0;
}

// ============================================================================
// Audio Implementation
// ============================================================================

int eif_hal_audio_init(const eif_audio_config_t *config) {
  if (!config)
    return -1;

  audio_state.sample_rate_hz =
      config->sample_rate_hz > 0 ? config->sample_rate_hz : 16000;
  audio_state.sample_count = 0;

  audio_state.config.mode = EIF_MOCK_AUDIO_TONE;
  audio_state.config.sample_rate = audio_state.sample_rate_hz;
  audio_state.config.frequency = 440.0f; // A4 note
  audio_state.config.amplitude = 0.5f;

  audio_state.initialized = true;
  return 0;
}

int eif_hal_audio_read(int16_t *buffer, int num_samples) {
  if (!audio_state.initialized || !buffer)
    return -1;

  float freq = audio_state.config.frequency;
  float amp = audio_state.config.amplitude;

  for (int i = 0; i < num_samples; i++) {
    float t =
        (float)(audio_state.sample_count + i) / audio_state.sample_rate_hz;
    float sample = amp * sinf(2 * M_PI * freq * t);

    // Add harmonics for more realistic sound
    sample += 0.3f * amp * sinf(2 * M_PI * freq * 2 * t);
    sample += 0.1f * amp * sinf(2 * M_PI * freq * 3 * t);

    // Add slight noise
    sample += (float)(rand() % 1000 - 500) / 50000.0f;

    // Convert to int16
    buffer[i] = (int16_t)(sample * 32767.0f);
  }

  audio_state.sample_count += num_samples;
  return num_samples;
}

int eif_hal_audio_read_float(float *buffer, int num_samples) {
  if (!audio_state.initialized || !buffer)
    return -1;

  float freq = audio_state.config.frequency;
  float amp = audio_state.config.amplitude;

  for (int i = 0; i < num_samples; i++) {
    float t =
        (float)(audio_state.sample_count + i) / audio_state.sample_rate_hz;
    buffer[i] = amp * sinf(2 * M_PI * freq * t);
    buffer[i] += (float)(rand() % 1000 - 500) / 50000.0f;
  }

  audio_state.sample_count += num_samples;
  return num_samples;
}

bool eif_hal_audio_available(void) { return audio_state.initialized; }

// ============================================================================
// GPIO Implementation
// ============================================================================

void eif_hal_gpio_init(int pin, eif_gpio_mode_t mode) {
  if (pin >= 0 && pin < 64) {
    gpio_state.mode[pin] = mode;
    gpio_state.value[pin] = 0;
  }
}

void eif_hal_gpio_write(int pin, int value) {
  if (pin >= 0 && pin < 64) {
    gpio_state.value[pin] = value ? 1 : 0;
  }
}

int eif_hal_gpio_read(int pin) {
  if (pin >= 0 && pin < 64) {
    return gpio_state.value[pin];
  }
  return 0;
}

void eif_hal_gpio_toggle(int pin) {
  if (pin >= 0 && pin < 64) {
    gpio_state.value[pin] = !gpio_state.value[pin];
  }
}

// ============================================================================
// Serial Implementation
// ============================================================================

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

static bool serial_initialized = false;

int eif_hal_serial_init(int baud_rate) {
  (void)baud_rate; // Not used for mock

  // Set stdin to non-blocking
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

  serial_initialized = true;
  return 0;
}

void eif_hal_serial_write(const char *str) {
  printf("%s", str);
  fflush(stdout);
}

int eif_hal_serial_readline(char *buffer, int max_len) {
  if (!serial_initialized || !buffer || max_len <= 0)
    return 0;

  struct pollfd pfd = {.fd = STDIN_FILENO, .events = POLLIN};
  if (poll(&pfd, 1, 0) <= 0)
    return 0;

  if (fgets(buffer, max_len, stdin)) {
    int len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
      len--;
    }
    return len;
  }
  return 0;
}

bool eif_hal_serial_available(void) {
  struct pollfd pfd = {.fd = STDIN_FILENO, .events = POLLIN};
  return poll(&pfd, 1, 0) > 0;
}

// ============================================================================
// I2C Implementation (Mock - does nothing)
// ============================================================================

int eif_hal_i2c_init(int sda_pin, int scl_pin, int freq_hz) {
  (void)sda_pin;
  (void)scl_pin;
  (void)freq_hz;
  return 0; // Mock always succeeds
}

int eif_hal_i2c_write(uint8_t addr, const uint8_t *data, int len) {
  (void)addr;
  (void)data;
  (void)len;
  return len; // Mock always succeeds
}

int eif_hal_i2c_read(uint8_t addr, uint8_t *data, int len) {
  (void)addr;
  memset(data, 0, len); // Return zeros
  return len;
}

int eif_hal_i2c_write_read(uint8_t addr, uint8_t reg, uint8_t *data, int len) {
  (void)addr;
  (void)reg;
  memset(data, 0, len);
  return len;
}

// ============================================================================
// Platform Info
// ============================================================================

const char *eif_hal_get_platform_name(void) { return "Mock HAL (PC)"; }

int eif_hal_get_free_memory(void) {
  return 1024 * 1024; // Report 1MB free (arbitrary for mock)
}
