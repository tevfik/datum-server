#ifndef EIF_VAD_ADVANCED_H
#define EIF_VAD_ADVANCED_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EIF_VAD_ADV_SILENCE = 0,
    EIF_VAD_ADV_ACTIVE
} eif_vad_adv_state_t;

/**
 * @brief Advanced VAD Configuration
 */
typedef struct {
    float energy_threshold;     ///< Minimum energy for voice
    float zcr_threshold;        ///< Minimum zero-crossing rate for voice-like signal (fricatives)
    int frame_size;             ///< Number of samples per frame
    int sample_rate;            ///< Sampling rate (e.g. 16000)
    int hangvoer_frames;        ///< How many frames to stay active after voice stops
} eif_vad_adv_config_t;

/**
 * @brief Advanced VAD Instance
 */
typedef struct {
    eif_vad_adv_config_t config;
    eif_vad_adv_state_t state;
    int hangvoer_counter;
    float noise_floor_energy;
} eif_vad_adv_t;

/**
 * @brief Initialize Advanced VAD
 */
void eif_vad_adv_init(eif_vad_adv_t *vad, eif_vad_adv_config_t *config);

/**
 * @brief Process a frame of audio
 * @param vad VAD instance
 * @param samples Audio samples (int16)
 * @param num_samples Number of samples
 * @return True if voice detected, False otherwise
 */
bool eif_vad_adv_process(eif_vad_adv_t *vad, const int16_t *samples, int num_samples);

/**
 * @brief Calculate Zero Crossing Rate
 */
float eif_calculate_zcr(const int16_t *samples, int num_samples);

#ifdef __cplusplus
}
#endif

#endif // EIF_VAD_ADVANCED_H
