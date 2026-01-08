/**
 * @file eif_power.h
 * @brief Power Profiler API for Edge AI
 * 
 * Track and optimize power consumption for embedded inference:
 * - Per-layer energy estimation
 * - Memory access tracking (major power consumer)
 * - Compute operations counting
 * - Battery life estimation
 * 
 * Use cases:
 * - Battery-powered IoT devices
 * - Energy-efficient deployment
 * - Model optimization feedback
 */

#ifndef EIF_POWER_H
#define EIF_POWER_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Constants
// =============================================================================

// Energy costs (in nanojoules, approximate for Cortex-M4)
#define EIF_ENERGY_INT8_MAC      1     // INT8 multiply-accumulate
#define EIF_ENERGY_FP32_MAC      10    // FP32 multiply-accumulate
#define EIF_ENERGY_SRAM_READ     5     // Read from SRAM
#define EIF_ENERGY_SRAM_WRITE    6     // Write to SRAM
#define EIF_ENERGY_FLASH_READ    10    // Read from Flash

// =============================================================================
// Types
// =============================================================================

/**
 * @brief Operation type for energy tracking
 */
typedef enum {
    EIF_OP_MAC_INT8,
    EIF_OP_MAC_FP32,
    EIF_OP_ADD,
    EIF_OP_MUL,
    EIF_OP_DIV,
    EIF_OP_EXP,
    EIF_OP_RELU,
    EIF_OP_SOFTMAX,
    EIF_OP_MEMORY_READ,
    EIF_OP_MEMORY_WRITE
} eif_op_type_t;

/**
 * @brief Per-layer power statistics
 */
typedef struct {
    const char* name;
    uint64_t mac_ops;           /**< Multiply-accumulate operations */
    uint64_t memory_reads;      /**< Bytes read */
    uint64_t memory_writes;     /**< Bytes written */
    uint64_t energy_nj;         /**< Estimated energy (nanojoules) */
    uint32_t latency_us;        /**< Measured latency (microseconds) */
} eif_layer_profile_t;

/**
 * @brief Model power profile
 */
typedef struct {
    eif_layer_profile_t* layers;
    int num_layers;
    int max_layers;
    
    // Totals
    uint64_t total_mac_ops;
    uint64_t total_memory_reads;
    uint64_t total_memory_writes;
    uint64_t total_energy_nj;
    uint32_t total_latency_us;
    
    // Per-inference stats
    uint32_t inference_count;
    
    eif_memory_pool_t* pool;
} eif_power_profile_t;

/**
 * @brief Inference timer (platform-specific)
 */
typedef struct {
    uint32_t start_cycles;
    uint32_t end_cycles;
    uint32_t cpu_freq_mhz;
} eif_timer_t;

// =============================================================================
// Power Profiler API
// =============================================================================

/**
 * @brief Initialize power profiler
 */
eif_status_t eif_power_init(eif_power_profile_t* profile,
                             int max_layers,
                             eif_memory_pool_t* pool);

/**
 * @brief Start profiling a layer
 */
eif_status_t eif_power_layer_start(eif_power_profile_t* profile,
                                    const char* layer_name);

/**
 * @brief Record operations for current layer
 */
eif_status_t eif_power_record_ops(eif_power_profile_t* profile,
                                   eif_op_type_t op_type,
                                   uint64_t count);

/**
 * @brief End layer profiling
 */
eif_status_t eif_power_layer_end(eif_power_profile_t* profile,
                                  uint32_t latency_us);

/**
 * @brief Estimate layer energy from dimensions (for planning)
 */
uint64_t eif_power_estimate_conv2d(int in_h, int in_w, int in_c,
                                    int out_c, int kernel_size,
                                    bool is_int8);

/**
 * @brief Estimate layer energy for fully connected
 */
uint64_t eif_power_estimate_fc(int in_features, int out_features, bool is_int8);

/**
 * @brief Get total energy for all layers
 */
uint64_t eif_power_total_energy(const eif_power_profile_t* profile);

/**
 * @brief Estimate battery life (hours)
 * @param battery_mwh Battery capacity in mWh
 * @param inferences_per_sec Inference rate
 */
float32_t eif_power_battery_life(const eif_power_profile_t* profile,
                                  float32_t battery_mwh,
                                  float32_t inferences_per_sec);

/**
 * @brief Print profile summary
 */
void eif_power_print_summary(const eif_power_profile_t* profile);

/**
 * @brief Reset profile for new inference
 */
void eif_power_reset(eif_power_profile_t* profile);

// =============================================================================
// Timer API (platform-specific implementation expected)
// =============================================================================

/**
 * @brief Start timer
 */
void eif_timer_start(eif_timer_t* timer);

/**
 * @brief Stop timer and get elapsed microseconds
 */
uint32_t eif_timer_stop(eif_timer_t* timer);

// =============================================================================
// Convenience Macros
// =============================================================================

#define EIF_PROFILE_LAYER(profile, name, code) do { \
    eif_timer_t _timer; \
    eif_timer_start(&_timer); \
    eif_power_layer_start(profile, name); \
    code; \
    uint32_t _us = eif_timer_stop(&_timer); \
    eif_power_layer_end(profile, _us); \
} while(0)

#ifdef __cplusplus
}
#endif

#endif // EIF_POWER_H
