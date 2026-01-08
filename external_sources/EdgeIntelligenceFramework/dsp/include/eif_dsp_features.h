/**
 * @file eif_dsp_features.h
 * @brief Signal Feature Extraction
 * 
 * MFCC, spectral features, time-domain features, VAD.
 */

#ifndef EIF_DSP_FEATURES_H
#define EIF_DSP_FEATURES_H

#include "eif_types.h"
#include "eif_status.h"
#include "eif_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// MFCC (Mel-Frequency Cepstral Coefficients)
// ============================================================================

typedef struct {
    int num_mfcc;
    int num_filters;
    int fft_length;
    int sample_rate;
    float32_t low_freq;
    float32_t high_freq;
    float32_t* filter_bank;
    float32_t* mel_energies;
} eif_mfcc_config_t;

eif_status_t eif_dsp_mfcc_init_f32(eif_mfcc_config_t* config, eif_memory_pool_t* pool);
eif_status_t eif_dsp_mfcc_compute_f32(const eif_mfcc_config_t* config, const float32_t* fft_mag, 
                                       float32_t* mfcc_out, eif_memory_pool_t* pool);

// ============================================================================
// Time-Domain Features
// ============================================================================

float32_t eif_dsp_rms_f32(const float32_t* input, size_t length);
float32_t eif_dsp_zcr_f32(const float32_t* input, size_t length);

// ============================================================================
// Spectral Features
// ============================================================================

float32_t eif_dsp_spectral_centroid_f32(const float32_t* input_mag, int fft_size, float32_t sample_rate);
float32_t eif_dsp_spectral_rolloff_f32(const float32_t* input_mag, int fft_size, 
                                        float32_t sample_rate, float32_t threshold_percent);
float32_t eif_dsp_spectral_flux_f32(const float32_t* input_mag, const float32_t* prev_mag, int fft_size);

// ============================================================================
// Peak and Envelope Detection
// ============================================================================

int eif_dsp_peak_detection_f32(const float32_t* input, int length, float32_t threshold, 
                                int min_distance, int* output_indices, int max_peaks);
void eif_dsp_envelope_f32(const float32_t* input, float32_t* output, int length, float32_t decay_factor);

// ============================================================================
// Voice Activity Detection (VAD)
// ============================================================================

bool eif_dsp_vad_energy_f32(const float32_t* input, size_t length, float32_t threshold);
bool eif_dsp_vad_zcr_f32(const float32_t* input, size_t length, float32_t threshold);

#ifdef __cplusplus
}
#endif

#endif // EIF_DSP_FEATURES_H
