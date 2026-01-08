# STM32 Optimization Guide

Complete guide for deploying Edge Intelligence Framework on STM32 microcontrollers.

## STM32 Family Comparison

| Series | Core | FPU | RAM | Flash | Best For |
|--------|------|-----|-----|-------|----------|
| STM32F0 | M0 | No | 4-32KB | 16-256KB | Simple DSP |
| STM32F1 | M3 | No | 6-96KB | 16-512KB | Basic filtering |
| STM32F4 | M4 | SP | 64-384KB | 256-2MB | Full NN, DSP |
| STM32F7 | M7 | DP | 256-512KB | 512-2MB | Complex AI |
| STM32H7 | M7 | DP | 1MB+ | 1-2MB | Edge AI powerhouse |
| STM32L4 | M4 | SP | 64-320KB | 256-1MB | Low-power AI |
| STM32U5 | M33 | SP | 768KB | 2MB | TrustZone + AI |

## Memory Layout

### Typical Configuration (STM32F4)
```c
// Memory regions
#define FLASH_START    0x08000000
#define SRAM_START     0x20000000
#define CCM_START      0x10000000  // Core-Coupled Memory (fast)

// Model weights in Flash
const float weights[] __attribute__((section(".rodata"))) = {
    // ... weights ...
};

// Activations in CCM (fastest access)
float activations[1024] __attribute__((section(".ccmram")));

// Scratch buffer in regular SRAM
float scratch[512];
```

### Linker Script Additions
```ld
/* Add CCM section for STM32F4 */
.ccmram :
{
    . = ALIGN(4);
    *(.ccmram)
    . = ALIGN(4);
} >CCMRAM
```

## Fixed-Point for M0/M3

For cores without FPU, use Q15 fixed-point:

```c
#include "eif_dsp_fir_fixed.h"
#include "eif_dsp_biquad_fixed.h"

// Q15 FIR filter
eif_fir_q15_t fir;
int16_t coeffs[16];
eif_fir_q15_init(&fir, coeffs, 16);

int16_t output = eif_fir_q15_process(&fir, input);
```

## CMSIS-DSP Integration

STM32 CubeMX includes CMSIS-DSP. Use when available:

```c
#include "arm_math.h"

// Use CMSIS-DSP FFT
arm_rfft_fast_instance_f32 fft;
arm_rfft_fast_init_f32(&fft, 256);
arm_rfft_fast_f32(&fft, input, output, 0);

// Use CMSIS-DSP filters
arm_biquad_cascade_df1_f32(&filter, input, output, block_size);
```

### Fallback to EIF
```c
#if defined(ARM_MATH_CM4) || defined(ARM_MATH_CM7)
    // Use CMSIS-DSP
    arm_rfft_fast_f32(&fft, input, output, 0);
#else
    // Use EIF pure C implementation
    eif_rfft(input, output, fft_size);
#endif
```

## DMA for Audio Processing

Use DMA for continuous audio capture:

```c
// Double buffer for I2S audio
#define AUDIO_BUFFER_SIZE 512
int16_t audio_buffer[2][AUDIO_BUFFER_SIZE];
volatile int active_buffer = 0;

// DMA callback
void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s) {
    // First half ready - process buffer 0
    process_audio(audio_buffer[0], AUDIO_BUFFER_SIZE / 2);
}

void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s) {
    // Second half ready - process buffer 1
    process_audio(audio_buffer[1], AUDIO_BUFFER_SIZE / 2);
}
```

## Low-Power Operation

### STM32L4 Power Modes
```c
// Before inference
HAL_PWREx_EnableLowPowerRunMode();

// Run inference at reduced clock
SystemClock_Config_LowPower();  // 2 MHz MSI

// Perform inference
eif_nn_forward(model, input, output);

// Restore full speed if needed
SystemClock_Config_FullSpeed();  // 80 MHz
```

### Sleep Between Inferences
```c
// Configure LPTIM for periodic wakeup
__HAL_RCC_LPTIM1_CLK_ENABLE();
HAL_LPTIM_TimeOut_Start_IT(&hlptim1, 0xFFFF, 1000);  // 1 second

// Enter STOP2 mode
HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
```

## CubeAI Integration

For TFLite models, use STM32Cube.AI:

```bash
# Convert model
stm32ai generate -m model.tflite -o generated

# Generated files:
# - network.h
# - network.c
# - network_data.h
# - network_data.c
```

### Using with EIF
```c
#include "network.h"
#include "eif_activity.h"

// Use Cube.AI for main inference
ai_handle network;
ai_network_create(&network, AI_NETWORK_DATA_CONFIG);

// Use EIF for preprocessing
eif_activity_features_t features;
eif_activity_extract_features(samples, 128, &features);

// Convert features to NN input
float nn_input[15];
eif_activity_features_to_array(&features, nn_input);

// Run Cube.AI inference
ai_network_run(network, nn_input, nn_output);
```

## Performance Tips

### 1. Use Flash XIP for Weights
```c
// Weights stay in Flash, no copy to RAM
const float __attribute__((section(".rodata"))) weights[4096] = {...};
```

### 2. Optimize Critical Loops
```c
// IAR/Keil: Use pragma for loop unrolling
#pragma unroll(4)
for (int i = 0; i < size; i++) {
    output[i] = input[i] * scale;
}
```

### 3. Use FPU Correctly
```c
// Enable FPU in startup
SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));

// Use float, not double
float result = sinf(x);  // ✓ Uses FPU
double result = sin(x);  // ✗ Software emulation
```

### 4. Align Buffers
```c
// 4-byte alignment for DMA and SIMD
float __attribute__((aligned(4))) buffer[256];
```

## Recommended Configurations

### Keyword Spotting (STM32L4R5)
```c
#define SAMPLE_RATE 16000
#define FRAME_SIZE 400      // 25ms
#define HOP_SIZE 160        // 10ms
#define NUM_MFCC 13
#define MODEL_SIZE 50000    // 50KB model
#define INFERENCE_INTERVAL_MS 100
```

### Anomaly Detection (STM32F407)
```c
#define SENSOR_RATE 1000    // Hz
#define WINDOW_SIZE 256
#define NUM_FEATURES 16
#define THRESHOLD_ALPHA 0.1f
```

### Activity Recognition (STM32L476)
```c
#define IMU_RATE 50         // Hz
#define WINDOW_SIZE 128     // 2.56 seconds
#define HOP_SIZE 64         // 1.28 seconds
#define NUM_CLASSES 7
```

## CubeMX Configuration

### Enabling FPU (CubeMX)
1. Project Manager → Code Generator
2. Enable "Use float with printf from newlib-nano"
3. Enable FPU in project settings

### DMA for ADC
1. System Core → DMA → Add DMA Request
2. ADC1 → Circular Mode
3. Memory Increment: Enable
4. Data Width: Half Word (16-bit)

## Build with STM32CubeIDE

```cmake
# Add EIF to your project
set(EIF_PATH "${CMAKE_SOURCE_DIR}/Middlewares/EIF")

include_directories(
    ${EIF_PATH}/core/include
    ${EIF_PATH}/dsp/include
    ${EIF_PATH}/ml/include
    ${EIF_PATH}/dl/include
)

# EIF is header-only, no libraries needed
```

## Example: Complete Sensor Pipeline

```c
#include "eif_dsp_smooth.h"
#include "eif_adaptive_threshold.h"
#include "eif_sensor_fusion.h"

eif_ema_t ema;
eif_z_threshold_t threshold;

void sensor_init(void) {
    eif_ema_init(&ema, 0.2f);
    eif_z_threshold_init(&threshold, 0.1f, 3.0f);
}

void sensor_process(float raw_value) {
    // Smooth
    float smoothed = eif_ema_update(&ema, raw_value);
    
    // Check for anomaly
    if (eif_z_threshold_check(&threshold, smoothed)) {
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
        // Trigger alarm
    }
}
```
