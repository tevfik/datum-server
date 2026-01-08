/**
 * @file eif_power.c
 * @brief Power Profiler Implementation
 */

#include "eif_power.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// Power Profiler Implementation
// =============================================================================

eif_status_t eif_power_init(eif_power_profile_t* profile,
                             int max_layers,
                             eif_memory_pool_t* pool) {
    if (!profile || !pool || max_layers <= 0) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    profile->layers = (eif_layer_profile_t*)eif_memory_alloc(pool,
        max_layers * sizeof(eif_layer_profile_t), sizeof(void*));
    if (!profile->layers) {
        return EIF_STATUS_OUT_OF_MEMORY;
    }
    
    profile->num_layers = 0;
    profile->max_layers = max_layers;
    profile->total_mac_ops = 0;
    profile->total_memory_reads = 0;
    profile->total_memory_writes = 0;
    profile->total_energy_nj = 0;
    profile->total_latency_us = 0;
    profile->inference_count = 0;
    profile->pool = pool;
    
    memset(profile->layers, 0, max_layers * sizeof(eif_layer_profile_t));
    
    return EIF_STATUS_OK;
}

eif_status_t eif_power_layer_start(eif_power_profile_t* profile,
                                    const char* layer_name) {
    if (!profile || profile->num_layers >= profile->max_layers) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    eif_layer_profile_t* layer = &profile->layers[profile->num_layers];
    layer->name = layer_name;
    layer->mac_ops = 0;
    layer->memory_reads = 0;
    layer->memory_writes = 0;
    layer->energy_nj = 0;
    layer->latency_us = 0;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_power_record_ops(eif_power_profile_t* profile,
                                   eif_op_type_t op_type,
                                   uint64_t count) {
    if (!profile) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    eif_layer_profile_t* layer = &profile->layers[profile->num_layers];
    
    uint64_t energy = 0;
    
    switch (op_type) {
        case EIF_OP_MAC_INT8:
            layer->mac_ops += count;
            energy = count * EIF_ENERGY_INT8_MAC;
            break;
        case EIF_OP_MAC_FP32:
            layer->mac_ops += count;
            energy = count * EIF_ENERGY_FP32_MAC;
            break;
        case EIF_OP_MEMORY_READ:
            layer->memory_reads += count;
            energy = count * EIF_ENERGY_SRAM_READ;
            break;
        case EIF_OP_MEMORY_WRITE:
            layer->memory_writes += count;
            energy = count * EIF_ENERGY_SRAM_WRITE;
            break;
        default:
            energy = count;  // Default 1 nJ per op
            break;
    }
    
    layer->energy_nj += energy;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_power_layer_end(eif_power_profile_t* profile,
                                  uint32_t latency_us) {
    if (!profile) {
        return EIF_STATUS_INVALID_ARGUMENT;
    }
    
    eif_layer_profile_t* layer = &profile->layers[profile->num_layers];
    layer->latency_us = latency_us;
    
    // Update totals
    profile->total_mac_ops += layer->mac_ops;
    profile->total_memory_reads += layer->memory_reads;
    profile->total_memory_writes += layer->memory_writes;
    profile->total_energy_nj += layer->energy_nj;
    profile->total_latency_us += latency_us;
    
    profile->num_layers++;
    
    return EIF_STATUS_OK;
}

uint64_t eif_power_estimate_conv2d(int in_h, int in_w, int in_c,
                                    int out_c, int kernel_size,
                                    bool is_int8) {
    // MACs = out_h * out_w * out_c * in_c * k * k
    int out_h = in_h - kernel_size + 1;  // Assuming no padding, stride 1
    int out_w = in_w - kernel_size + 1;
    
    uint64_t macs = (uint64_t)out_h * out_w * out_c * in_c * kernel_size * kernel_size;
    
    uint64_t energy = macs * (is_int8 ? EIF_ENERGY_INT8_MAC : EIF_ENERGY_FP32_MAC);
    
    // Add memory access energy
    uint64_t input_reads = in_h * in_w * in_c;
    uint64_t weight_reads = out_c * in_c * kernel_size * kernel_size;
    uint64_t output_writes = out_h * out_w * out_c;
    
    energy += input_reads * EIF_ENERGY_SRAM_READ;
    energy += weight_reads * EIF_ENERGY_FLASH_READ;  // Weights often in Flash
    energy += output_writes * EIF_ENERGY_SRAM_WRITE;
    
    return energy;
}

uint64_t eif_power_estimate_fc(int in_features, int out_features, bool is_int8) {
    uint64_t macs = (uint64_t)in_features * out_features;
    
    uint64_t energy = macs * (is_int8 ? EIF_ENERGY_INT8_MAC : EIF_ENERGY_FP32_MAC);
    
    // Memory access
    energy += in_features * EIF_ENERGY_SRAM_READ;
    energy += (uint64_t)in_features * out_features * EIF_ENERGY_FLASH_READ;
    energy += out_features * EIF_ENERGY_SRAM_WRITE;
    
    return energy;
}

uint64_t eif_power_total_energy(const eif_power_profile_t* profile) {
    if (!profile) return 0;
    return profile->total_energy_nj;
}

float32_t eif_power_battery_life(const eif_power_profile_t* profile,
                                  float32_t battery_mwh,
                                  float32_t inferences_per_sec) {
    if (!profile || profile->total_energy_nj == 0 || inferences_per_sec <= 0) {
        return 0.0f;
    }
    
    // Convert nJ to mWh: 1 mWh = 3.6e9 nJ
    float32_t energy_per_inference_mwh = profile->total_energy_nj / 3.6e9f;
    
    // Energy per second
    float32_t power_mw = energy_per_inference_mwh * inferences_per_sec * 3600.0f;
    
    // Battery life in hours
    float32_t hours = battery_mwh / power_mw;
    
    return hours;
}

void eif_power_print_summary(const eif_power_profile_t* profile) {
    if (!profile) return;
    
    printf("\n=== Power Profile Summary ===\n");
    printf("Layers: %d\n", profile->num_layers);
    printf("Total MACs:    %llu\n", (unsigned long long)profile->total_mac_ops);
    printf("Memory Reads:  %llu bytes\n", (unsigned long long)profile->total_memory_reads);
    printf("Memory Writes: %llu bytes\n", (unsigned long long)profile->total_memory_writes);
    printf("Total Energy:  %llu nJ (%.3f mJ)\n", 
           (unsigned long long)profile->total_energy_nj,
           profile->total_energy_nj / 1e6f);
    printf("Total Latency: %u us\n", profile->total_latency_us);
    
    printf("\nPer-Layer Breakdown:\n");
    printf("%-20s %12s %12s %10s\n", "Layer", "MACs", "Energy(nJ)", "Time(us)");
    printf("%-20s %12s %12s %10s\n", "--------------------", "------------", "------------", "----------");
    
    for (int i = 0; i < profile->num_layers; i++) {
        eif_layer_profile_t* layer = &profile->layers[i];
        printf("%-20s %12llu %12llu %10u\n",
               layer->name ? layer->name : "unknown",
               (unsigned long long)layer->mac_ops,
               (unsigned long long)layer->energy_nj,
               layer->latency_us);
    }
    printf("\n");
}

void eif_power_reset(eif_power_profile_t* profile) {
    if (!profile) return;
    
    profile->num_layers = 0;
    profile->total_mac_ops = 0;
    profile->total_memory_reads = 0;
    profile->total_memory_writes = 0;
    profile->total_energy_nj = 0;
    profile->total_latency_us = 0;
    profile->inference_count++;
}

// =============================================================================
// Timer Implementation (Generic - can be overridden for specific platforms)
// =============================================================================

#include <time.h>

static uint64_t get_microseconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static uint64_t timer_start_us;

void eif_timer_start(eif_timer_t* timer) {
    if (!timer) return;
    timer_start_us = get_microseconds();
    timer->start_cycles = 0;  // Not used in generic implementation
}

uint32_t eif_timer_stop(eif_timer_t* timer) {
    uint64_t end_us = get_microseconds();
    return (uint32_t)(end_us - timer_start_us);
}
