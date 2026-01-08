/**
 * @file eif_bf_particle.h
 * @brief Particle Filter
 * 
 * Sequential Monte Carlo / Particle Filter implementation.
 */

#ifndef EIF_BF_PARTICLE_H
#define EIF_BF_PARTICLE_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Particle Filter
// ============================================================================

typedef void (*eif_pf_transition_func)(const float32_t* state, const float32_t* control, float32_t* next_state);
typedef float32_t (*eif_pf_likelihood_func)(const float32_t* state, const float32_t* measurement);

typedef struct {
    int state_dim;
    int num_particles;
    
    float32_t* particles;        ///< num_particles * state_dim
    float32_t* weights;          ///< num_particles
    float32_t* process_noise;    ///< state_dim (std dev)
    
    eif_pf_transition_func transition_func;
    eif_pf_likelihood_func likelihood_func;
    
    float32_t* scratch;          ///< For resampling
} eif_particle_filter_t;

eif_status_t eif_pf_init(eif_particle_filter_t* pf, const float32_t* init_state, 
                          const float32_t* init_cov, eif_memory_pool_t* pool);
eif_status_t eif_pf_predict(eif_particle_filter_t* pf, const float32_t* control);
eif_status_t eif_pf_update(eif_particle_filter_t* pf, const float32_t* measurement);
eif_status_t eif_pf_resample(eif_particle_filter_t* pf);
eif_status_t eif_pf_estimate(const eif_particle_filter_t* pf, float32_t* state_est);

#ifdef __cplusplus
}
#endif

#endif // EIF_BF_PARTICLE_H
