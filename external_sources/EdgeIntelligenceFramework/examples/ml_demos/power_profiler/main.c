/**
 * @file main.c
 * @brief Power Profiler Demo - Battery Life Estimation
 * 
 * Demonstrates:
 * 1. Per-layer energy tracking
 * 2. Memory access profiling
 * 3. Battery life estimation
 * 4. Comparison of INT8 vs FP32 efficiency
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eif_power.h"
#include "eif_memory.h"

// Memory pool
static uint8_t pool_buffer[32 * 1024];
static eif_memory_pool_t pool;

// Simulated inference function
void simulate_conv_layer(const char* name, int in_h, int in_w, int in_c,
                          int out_c, int kernel, bool is_int8,
                          eif_power_profile_t* profile) {
    eif_timer_t timer;
    eif_timer_start(&timer);
    
    eif_power_layer_start(profile, name);
    
    // Calculate MACs
    int out_h = in_h - kernel + 1;
    int out_w = in_w - kernel + 1;
    uint64_t macs = (uint64_t)out_h * out_w * out_c * in_c * kernel * kernel;
    
    // Record operations
    if (is_int8) {
        eif_power_record_ops(profile, EIF_OP_MAC_INT8, macs);
    } else {
        eif_power_record_ops(profile, EIF_OP_MAC_FP32, macs);
    }
    
    // Memory reads (input + weights)
    uint64_t input_bytes = in_h * in_w * in_c * (is_int8 ? 1 : 4);
    uint64_t weight_bytes = out_c * in_c * kernel * kernel * (is_int8 ? 1 : 4);
    eif_power_record_ops(profile, EIF_OP_MEMORY_READ, input_bytes + weight_bytes);
    
    // Memory writes (output)
    uint64_t output_bytes = out_h * out_w * out_c * (is_int8 ? 1 : 4);
    eif_power_record_ops(profile, EIF_OP_MEMORY_WRITE, output_bytes);
    
    // Simulate some computation time (proportional to MACs)
    volatile float sum = 0;
    for (int i = 0; i < macs / 10000; i++) {
        sum += 1.0f;
    }
    
    uint32_t elapsed = eif_timer_stop(&timer);
    eif_power_layer_end(profile, elapsed);
}

void simulate_fc_layer(const char* name, int in_features, int out_features,
                        bool is_int8, eif_power_profile_t* profile) {
    eif_timer_t timer;
    eif_timer_start(&timer);
    
    eif_power_layer_start(profile, name);
    
    uint64_t macs = (uint64_t)in_features * out_features;
    
    if (is_int8) {
        eif_power_record_ops(profile, EIF_OP_MAC_INT8, macs);
    } else {
        eif_power_record_ops(profile, EIF_OP_MAC_FP32, macs);
    }
    
    uint64_t weight_bytes = in_features * out_features * (is_int8 ? 1 : 4);
    eif_power_record_ops(profile, EIF_OP_MEMORY_READ, weight_bytes);
    eif_power_record_ops(profile, EIF_OP_MEMORY_WRITE, out_features * (is_int8 ? 1 : 4));
    
    volatile float sum = 0;
    for (int i = 0; i < macs / 10000; i++) {
        sum += 1.0f;
    }
    
    uint32_t elapsed = eif_timer_stop(&timer);
    eif_power_layer_end(profile, elapsed);
}

int main(void) {
    printf("========================================\n");
    printf("Power Profiler Demo\n");
    printf("Battery Life Estimation\n");
    printf("========================================\n\n");
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // =========================================
    // Demo 1: FP32 Model
    // =========================================
    printf("=== Demo 1: FP32 Model (MobileNet-like) ===\n\n");
    
    eif_power_profile_t fp32_profile;
    eif_power_init(&fp32_profile, 10, &pool);
    
    // Simulate MobileNet-like architecture
    simulate_conv_layer("conv1", 28, 28, 1, 32, 3, false, &fp32_profile);
    simulate_conv_layer("conv2", 26, 26, 32, 64, 3, false, &fp32_profile);
    simulate_conv_layer("conv3", 24, 24, 64, 64, 3, false, &fp32_profile);
    simulate_fc_layer("fc1", 22*22*64, 128, false, &fp32_profile);
    simulate_fc_layer("fc2", 128, 10, false, &fp32_profile);
    
    eif_power_print_summary(&fp32_profile);
    
    // Battery estimation (100mAh smartwatch battery @ 3.7V = 370mWh)
    float32_t battery_mwh = 370.0f;
    float32_t fps = 1.0f;  // 1 inference per second
    
    float32_t fp32_hours = eif_power_battery_life(&fp32_profile, battery_mwh, fps);
    printf("Battery Life (370mWh, 1 FPS): %.1f hours\n\n", fp32_hours);
    
    // =========================================
    // Demo 2: INT8 Model
    // =========================================
    printf("=== Demo 2: INT8 Quantized Model ===\n\n");
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    eif_power_profile_t int8_profile;
    eif_power_init(&int8_profile, 10, &pool);
    
    // Same architecture but INT8
    simulate_conv_layer("conv1_int8", 28, 28, 1, 32, 3, true, &int8_profile);
    simulate_conv_layer("conv2_int8", 26, 26, 32, 64, 3, true, &int8_profile);
    simulate_conv_layer("conv3_int8", 24, 24, 64, 64, 3, true, &int8_profile);
    simulate_fc_layer("fc1_int8", 22*22*64, 128, true, &int8_profile);
    simulate_fc_layer("fc2_int8", 128, 10, true, &int8_profile);
    
    eif_power_print_summary(&int8_profile);
    
    float32_t int8_hours = eif_power_battery_life(&int8_profile, battery_mwh, fps);
    printf("Battery Life (370mWh, 1 FPS): %.1f hours\n\n", int8_hours);
    
    // =========================================
    // Demo 3: Energy Estimation (Planning)
    // =========================================
    printf("=== Demo 3: Energy Estimation for Planning ===\n\n");
    
    printf("Pre-deployment energy estimates:\n\n");
    
    // Estimate energy for different layer configurations
    printf("%-20s %15s %15s\n", "Layer", "FP32 (nJ)", "INT8 (nJ)");
    printf("%-20s %15s %15s\n", "--------------------", "---------------", "---------------");
    
    struct {
        const char* name;
        int in_h, in_w, in_c, out_c, kernel;
    } layers[] = {
        {"Conv 28x28x1 -> 32", 28, 28, 1, 32, 3},
        {"Conv 14x14x32 -> 64", 14, 14, 32, 64, 3},
        {"Conv 7x7x64 -> 128", 7, 7, 64, 128, 3},
    };
    
    for (int i = 0; i < 3; i++) {
        uint64_t fp32_energy = eif_power_estimate_conv2d(
            layers[i].in_h, layers[i].in_w, layers[i].in_c,
            layers[i].out_c, layers[i].kernel, false);
        uint64_t int8_energy = eif_power_estimate_conv2d(
            layers[i].in_h, layers[i].in_w, layers[i].in_c,
            layers[i].out_c, layers[i].kernel, true);
        
        printf("%-20s %15llu %15llu\n", layers[i].name,
               (unsigned long long)fp32_energy, (unsigned long long)int8_energy);
    }
    
    printf("\nFC Layers:\n");
    printf("%-20s %15s %15s\n", "Layer", "FP32 (nJ)", "INT8 (nJ)");
    printf("%-20s %15s %15s\n", "--------------------", "---------------", "---------------");
    
    printf("%-20s %15llu %15llu\n", "FC 1024 -> 256",
           (unsigned long long)eif_power_estimate_fc(1024, 256, false),
           (unsigned long long)eif_power_estimate_fc(1024, 256, true));
    printf("%-20s %15llu %15llu\n", "FC 256 -> 10",
           (unsigned long long)eif_power_estimate_fc(256, 10, false),
           (unsigned long long)eif_power_estimate_fc(256, 10, true));
    
    // =========================================
    // Summary
    // =========================================
    printf("\n========================================\n");
    printf("Summary\n");
    printf("========================================\n");
    printf("FP32 Total Energy:  %.3f mJ\n", fp32_profile.total_energy_nj / 1e6f);
    printf("INT8 Total Energy:  %.3f mJ\n", int8_profile.total_energy_nj / 1e6f);
    printf("Energy Savings:     %.1fx\n", 
           (float)fp32_profile.total_energy_nj / int8_profile.total_energy_nj);
    printf("\nFP32 Battery Life:  %.1f hours\n", fp32_hours);
    printf("INT8 Battery Life:  %.1f hours\n", int8_hours);
    printf("========================================\n\n");
    
    return 0;
}
