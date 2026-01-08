#include "eif_vad_advanced.h"
#include <math.h>

void eif_vad_adv_init(eif_vad_adv_t *vad, eif_vad_adv_config_t *config) {
    if (!vad || !config) return;
    vad->config = *config;
    vad->state = EIF_VAD_ADV_SILENCE;
    vad->hangvoer_counter = 0;
    vad->noise_floor_energy = config->energy_threshold * 0.1f;
}

float eif_calculate_zcr(const int16_t *samples, int num_samples) {
    if (!samples || num_samples < 2) return 0.0f;

    int crossings = 0;
    for (int i = 1; i < num_samples; i++) {
        // Detect sign change
        if ((samples[i-1] >= 0 && samples[i] < 0) || (samples[i-1] < 0 && samples[i] >= 0)) {
            crossings++;
        }
    }
    
    return (float)crossings / (float)(num_samples - 1);
}

bool eif_vad_adv_process(eif_vad_adv_t *vad, const int16_t *samples, int num_samples) {
    if (!vad || !samples || num_samples == 0) return false;

    // 1. Calculate Energy
    float energy = 0.0f;
    for (int i = 0; i < num_samples; i++) {
        float s = (float)samples[i] / 32768.0f;
        energy += s * s;
    }
    energy /= num_samples;

    // 2. Calculate ZCR
    float zcr = eif_calculate_zcr(samples, num_samples);

    // 3. Logic:
    // Voice usually has high energy.
    // However, fricatives (s, f, sh) might have lower energy but high ZCR.
    // Noise usually has random energy but can be high.
    
    // Adaptive noise floor (Simple IIR)
    if (energy < vad->config.energy_threshold) {
        vad->noise_floor_energy = 0.95f * vad->noise_floor_energy + 0.05f * energy;
    }

    bool energy_detect = energy > (vad->config.energy_threshold + vad->noise_floor_energy);
    
    // ZCR check: Unvoiced speech often has higher ZCR than voiced, but lower than white noise sometimes.
    // This is a simplistic check: if energy is moderate but ZCR is significant, it might be speech.
    
    // Often ZCR is used to distinguish Voiced vs Unvoiced, not just Speech vs Noise.
    // Here we use it to boost sensitivity if energy is borderline.
    bool zcr_boost = (zcr > vad->config.zcr_threshold) && (energy > vad->config.energy_threshold * 0.5f);

    bool is_speech = energy_detect || zcr_boost;

    if (is_speech) {
        vad->hangvoer_counter = vad->config.hangvoer_frames;
        vad->state = EIF_VAD_ADV_ACTIVE;
    } else {
        if (vad->hangvoer_counter > 0) {
            vad->hangvoer_counter--;
            vad->state = EIF_VAD_ADV_ACTIVE;
        } else {
            vad->state = EIF_VAD_ADV_SILENCE;
        }
    }

    return (vad->state == EIF_VAD_ADV_ACTIVE);
}
