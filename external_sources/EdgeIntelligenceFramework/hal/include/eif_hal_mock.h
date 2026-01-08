/**
 * @file eif_hal_mock.h
 * @brief Mock Hardware Definitions
 *
 * Provides types and constants for simulated hardware.
 * Functions are declared in eif_hal.h
 */

#ifndef EIF_HAL_MOCK_H
#define EIF_HAL_MOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Mock GPIO Definitions
// =============================================================================

#define EIF_GPIO_PORT_A 0
#define EIF_GPIO_PORT_B 1

typedef enum {
  EIF_GPIO_PIN_LED_RED = 0,
  EIF_GPIO_PIN_LED_GREEN = 1,
  EIF_GPIO_PIN_BUTTON_USER = 2
} eif_mock_gpio_pin_t;

// =============================================================================
// Mock IMU Configuration
// =============================================================================

typedef enum {
  EIF_MOCK_IMU_SINE = 0, // Sinusoidal motion
  EIF_MOCK_IMU_RANDOM,   // Random motion
  EIF_MOCK_IMU_STATIC    // Static (gravity only)
} eif_mock_imu_mode_t;

typedef struct {
  eif_mock_imu_mode_t mode;
  float sample_rate;
  float noise_level;
  float frequency; // For sinusoidal mode
  float amplitude; // For sinusoidal mode
} eif_mock_imu_config_t;

// =============================================================================
// Mock Audio Configuration
// =============================================================================

typedef enum {
  EIF_MOCK_AUDIO_TONE = 0, // Pure sine tone
  EIF_MOCK_AUDIO_NOISE,    // White noise
  EIF_MOCK_AUDIO_SPEECH    // Simulated speech (modulated)
} eif_mock_audio_mode_t;

typedef struct {
  eif_mock_audio_mode_t mode;
  int sample_rate;
  float frequency;
  float amplitude;
} eif_mock_audio_config_t;

#ifdef __cplusplus
}
#endif

#endif // EIF_HAL_MOCK_H
