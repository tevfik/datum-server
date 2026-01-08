/**
 * @file eif_hal.h
 * @brief Unified Hardware Abstraction Layer
 * 
 * This header provides a platform-agnostic interface for hardware access.
 * Compile with -DEIF_USE_MOCK_HAL to use mock implementations (PC testing).
 * Compile without to use real hardware implementations (ESP32, STM32, etc.).
 * 
 * Usage:
 *   // Real hardware (embedded)
 *   cc -DESP32 main.c -I hal/include
 *   
 *   // Mock hardware (PC testing)
 *   cc -DEIF_USE_MOCK_HAL main.c -I hal/include hal/platforms/generic/eif_hal_mock.c
 */

#ifndef EIF_HAL_H
#define EIF_HAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Platform Detection
// ============================================================================

#if defined(EIF_USE_MOCK_HAL)
    #define EIF_PLATFORM_MOCK 1
#elif defined(ESP32) || defined(ESP_PLATFORM)
    #define EIF_PLATFORM_ESP32 1
#elif defined(STM32F4) || defined(STM32_HAL)
    #define EIF_PLATFORM_STM32 1
#elif defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
    #define EIF_PLATFORM_PC 1
    #ifndef EIF_USE_MOCK_HAL
        #define EIF_USE_MOCK_HAL 1
        #define EIF_PLATFORM_MOCK 1
    #endif
#else
    #define EIF_PLATFORM_GENERIC 1
#endif

// ============================================================================
// IMU Interface
// ============================================================================

typedef struct {
    float ax, ay, az;   // Accelerometer (g)
    float gx, gy, gz;   // Gyroscope (rad/s or deg/s)
    float mx, my, mz;   // Magnetometer (optional, uT)
    uint32_t timestamp; // Microseconds
} eif_imu_data_t;

typedef struct {
    float sample_rate_hz;
    float accel_range_g;  // ±2, ±4, ±8, ±16
    float gyro_range_dps; // ±250, ±500, ±1000, ±2000
    bool use_magnetometer;
} eif_imu_config_t;

/**
 * @brief Initialize IMU with configuration
 * @return 0 on success, negative on error
 */
int eif_hal_imu_init(const eif_imu_config_t* config);

/**
 * @brief Read IMU data
 * @param data Output structure
 * @return 0 on success, negative on error
 */
int eif_hal_imu_read(eif_imu_data_t* data);

/**
 * @brief Check if IMU data is ready
 */
bool eif_hal_imu_data_ready(void);

/**
 * @brief Calibrate IMU (blocking, may take several seconds)
 */
int eif_hal_imu_calibrate(void);

// ============================================================================
// Audio Interface
// ============================================================================

typedef struct {
    int sample_rate_hz;    // 8000, 16000, 44100
    int bits_per_sample;   // 8, 16
    int channels;          // 1 (mono), 2 (stereo)
    int buffer_size_ms;    // Duration of each buffer
} eif_audio_config_t;

/**
 * @brief Initialize audio input
 */
int eif_hal_audio_init(const eif_audio_config_t* config);

/**
 * @brief Read audio samples (blocking)
 * @param buffer Output buffer (int16_t samples)
 * @param num_samples Number of samples to read
 * @return Number of samples read, negative on error
 */
int eif_hal_audio_read(int16_t* buffer, int num_samples);

/**
 * @brief Read audio samples as float (-1.0 to 1.0)
 */
int eif_hal_audio_read_float(float* buffer, int num_samples);

/**
 * @brief Check if audio data is available
 */
bool eif_hal_audio_available(void);

// ============================================================================
// GPIO Interface
// ============================================================================

typedef enum {
    EIF_GPIO_INPUT,
    EIF_GPIO_OUTPUT,
    EIF_GPIO_INPUT_PULLUP,
    EIF_GPIO_INPUT_PULLDOWN
} eif_gpio_mode_t;

/**
 * @brief Configure GPIO pin
 */
void eif_hal_gpio_init(int pin, eif_gpio_mode_t mode);

/**
 * @brief Write to GPIO pin
 */
void eif_hal_gpio_write(int pin, int value);

/**
 * @brief Read from GPIO pin
 */
int eif_hal_gpio_read(int pin);

/**
 * @brief Toggle GPIO pin
 */
void eif_hal_gpio_toggle(int pin);

// ============================================================================
// Timer Interface
// ============================================================================

/**
 * @brief Get current time in microseconds (wraps)
 */
uint32_t eif_hal_timer_us(void);

/**
 * @brief Get current time in milliseconds (wraps)
 */
uint32_t eif_hal_timer_ms(void);

/**
 * @brief Delay for specified microseconds
 */
void eif_hal_delay_us(uint32_t us);

/**
 * @brief Delay for specified milliseconds
 */
void eif_hal_delay_ms(uint32_t ms);

// ============================================================================
// Serial/UART Interface
// ============================================================================

/**
 * @brief Initialize serial port
 */
int eif_hal_serial_init(int baud_rate);

/**
 * @brief Write string to serial
 */
void eif_hal_serial_write(const char* str);

/**
 * @brief Read line from serial (non-blocking)
 * @return Number of bytes read, 0 if no data
 */
int eif_hal_serial_readline(char* buffer, int max_len);

/**
 * @brief Check if serial data is available
 */
bool eif_hal_serial_available(void);

// ============================================================================
// I2C Interface (for sensors)
// ============================================================================

/**
 * @brief Initialize I2C bus
 */
int eif_hal_i2c_init(int sda_pin, int scl_pin, int freq_hz);

/**
 * @brief Write bytes to I2C device
 */
int eif_hal_i2c_write(uint8_t addr, const uint8_t* data, int len);

/**
 * @brief Read bytes from I2C device
 */
int eif_hal_i2c_read(uint8_t addr, uint8_t* data, int len);

/**
 * @brief Write register then read (common pattern)
 */
int eif_hal_i2c_write_read(uint8_t addr, uint8_t reg, uint8_t* data, int len);

// ============================================================================
// LED Interface (convenience)
// ============================================================================

#ifndef EIF_LED_PIN
#define EIF_LED_PIN 2  // Common LED pin (ESP32 built-in)
#endif

static inline void eif_hal_led_init(void) {
    eif_hal_gpio_init(EIF_LED_PIN, EIF_GPIO_OUTPUT);
}

static inline void eif_hal_led_on(void) {
    eif_hal_gpio_write(EIF_LED_PIN, 1);
}

static inline void eif_hal_led_off(void) {
    eif_hal_gpio_write(EIF_LED_PIN, 0);
}

static inline void eif_hal_led_toggle(void) {
    eif_hal_gpio_toggle(EIF_LED_PIN);
}

static inline void eif_hal_led_blink(int times, int delay_ms) {
    for (int i = 0; i < times; i++) {
        eif_hal_led_on();
        eif_hal_delay_ms(delay_ms);
        eif_hal_led_off();
        eif_hal_delay_ms(delay_ms);
    }
}

// ============================================================================
// Platform Info
// ============================================================================

/**
 * @brief Get platform name string
 */
const char* eif_hal_get_platform_name(void);

/**
 * @brief Get free heap/memory size (if available)
 */
int eif_hal_get_free_memory(void);

#ifdef __cplusplus
}
#endif

#endif // EIF_HAL_H
