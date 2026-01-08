/**
 * @file eif_audio_intelligence.h
 * @brief Audio Intelligence for Edge Devices
 *
 * Lightweight audio processing for:
 * - Voice Activity Detection (VAD)
 * - Keyword Spotting (KWS) support
 * - Audio event detection
 *
 * Designed for always-on audio processing on microcontrollers.
 */

#ifndef EIF_AUDIO_INTELLIGENCE_H
#define EIF_AUDIO_INTELLIGENCE_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_AUDIO_FRAME_SIZE 256
#define EIF_AUDIO_NUM_MFCC 13
#define EIF_AUDIO_MAX_COMMAND_LEN 100

// =============================================================================
// Voice Activity Detection (VAD)
// =============================================================================

typedef enum {
  EIF_VAD_SILENCE = 0,
  EIF_VAD_MAYBE_VOICE,
  EIF_VAD_VOICE,
  EIF_VAD_TRAILING
} eif_vad_state_t;

/**
 * @brief Simple energy-based VAD
 */
typedef struct {
  float energy_threshold;  ///< Threshold for voice detection
  float silence_threshold; ///< Threshold for silence
  float noise_floor;       ///< Estimated noise floor
  float noise_alpha;       ///< Noise adaptation rate

  eif_vad_state_t state;
  int voice_count;         ///< Consecutive voice frames
  int silence_count;       ///< Consecutive silence frames
  int min_voice_frames;    ///< Minimum to confirm voice
  int max_trailing_frames; ///< Trailing edge after voice
} eif_vad_t;

/**
 * @brief Initialize VAD
 */
static inline void eif_vad_init(eif_vad_t *vad, float initial_threshold) {
  vad->energy_threshold = initial_threshold;
  vad->silence_threshold = initial_threshold * 0.5f;
  vad->noise_floor = initial_threshold * 0.25f;
  vad->noise_alpha = 0.01f;
  vad->state = EIF_VAD_SILENCE;
  vad->voice_count = 0;
  vad->silence_count = 0;
  vad->min_voice_frames = 3;
  vad->max_trailing_frames = 10;
}

/**
 * @brief Calculate frame energy
 */
static inline float eif_vad_frame_energy(const int16_t *samples,
                                         int num_samples) {
  float energy = 0.0f;
  for (int i = 0; i < num_samples; i++) {
    float s = (float)samples[i] / 32768.0f;
    energy += s * s;
  }
  return energy / num_samples;
}

/**
 * @brief Calculate frame energy (float samples)
 */
static inline float eif_vad_frame_energy_f(const float *samples,
                                           int num_samples) {
  float energy = 0.0f;
  for (int i = 0; i < num_samples; i++) {
    energy += samples[i] * samples[i];
  }
  return energy / num_samples;
}

/**
 * @brief Process frame and update VAD state
 * @return true if voice is currently detected
 */
static inline bool eif_vad_process(eif_vad_t *vad, float frame_energy) {
  bool is_above_thresh = frame_energy > vad->energy_threshold;

  // Update noise floor during silence
  if (vad->state == EIF_VAD_SILENCE && !is_above_thresh) {
    vad->noise_floor = (1.0f - vad->noise_alpha) * vad->noise_floor +
                       vad->noise_alpha * frame_energy;
    // Adapt threshold
    vad->energy_threshold = vad->noise_floor * 4.0f;
    vad->silence_threshold = vad->noise_floor * 2.0f;
  }

  // State machine
  switch (vad->state) {
  case EIF_VAD_SILENCE:
    if (is_above_thresh) {
      vad->voice_count++;
      if (vad->voice_count >= vad->min_voice_frames) {
        vad->state = EIF_VAD_VOICE;
      } else {
        vad->state = EIF_VAD_MAYBE_VOICE;
      }
    } else {
      vad->voice_count = 0;
    }
    break;

  case EIF_VAD_MAYBE_VOICE:
    if (is_above_thresh) {
      vad->voice_count++;
      if (vad->voice_count >= vad->min_voice_frames) {
        vad->state = EIF_VAD_VOICE;
      }
    } else {
      vad->state = EIF_VAD_SILENCE;
      vad->voice_count = 0;
    }
    break;

  case EIF_VAD_VOICE:
    if (frame_energy < vad->silence_threshold) {
      vad->silence_count++;
      if (vad->silence_count >= vad->max_trailing_frames) {
        vad->state = EIF_VAD_SILENCE;
        vad->voice_count = 0;
        vad->silence_count = 0;
      } else {
        vad->state = EIF_VAD_TRAILING;
      }
    } else {
      vad->silence_count = 0;
    }
    break;

  case EIF_VAD_TRAILING:
    if (is_above_thresh) {
      vad->state = EIF_VAD_VOICE;
      vad->silence_count = 0;
    } else {
      vad->silence_count++;
      if (vad->silence_count >= vad->max_trailing_frames) {
        vad->state = EIF_VAD_SILENCE;
        vad->voice_count = 0;
        vad->silence_count = 0;
      }
    }
    break;
  }

  return (vad->state == EIF_VAD_VOICE || vad->state == EIF_VAD_TRAILING);
}

// =============================================================================
// Simple Keyword Spotting Support
// =============================================================================

/**
 * @brief Keyword spotting feature buffer (for NN input)
 */
typedef struct {
  float features[40][EIF_AUDIO_NUM_MFCC]; // 40 frames of MFCC
  int frame_idx;
  int total_frames;
  bool buffer_full;
} eif_kws_buffer_t;

/**
 * @brief Initialize KWS buffer
 */
static inline void eif_kws_buffer_init(eif_kws_buffer_t *buf) {
  buf->frame_idx = 0;
  buf->total_frames = 0;
  buf->buffer_full = false;
}

/**
 * @brief Add MFCC frame to buffer
 */
static inline bool eif_kws_buffer_add(eif_kws_buffer_t *buf, const float *mfcc,
                                      int num_coeffs) {
  int n = num_coeffs < EIF_AUDIO_NUM_MFCC ? num_coeffs : EIF_AUDIO_NUM_MFCC;

  for (int i = 0; i < n; i++) {
    buf->features[buf->frame_idx][i] = mfcc[i];
  }

  buf->frame_idx = (buf->frame_idx + 1) % 40;
  buf->total_frames++;

  if (buf->total_frames >= 40) {
    buf->buffer_full = true;
  }

  return buf->buffer_full;
}

/**
 * @brief Get flattened features for NN input
 */
static inline void eif_kws_buffer_flatten(eif_kws_buffer_t *buf,
                                          float *output) {
  int start = (buf->frame_idx) % 40; // Oldest frame
  int idx = 0;

  for (int t = 0; t < 40; t++) {
    int frame_idx = (start + t) % 40;
    for (int c = 0; c < EIF_AUDIO_NUM_MFCC; c++) {
      output[idx++] = buf->features[frame_idx][c];
    }
  }
}

// =============================================================================
// Audio Event Detection
// =============================================================================

/**
 * @brief Simple audio event (loud sound) detector
 */
typedef struct {
  float threshold;
  float alpha;
  float baseline;
  int debounce_frames;
  int frames_since_event;
  bool event_active;
} eif_audio_event_t;

/**
 * @brief Initialize audio event detector
 */
static inline void eif_audio_event_init(eif_audio_event_t *ae, float threshold,
                                        int debounce_frames) {
  ae->threshold = threshold;
  ae->alpha = 0.01f;
  ae->baseline = 0.0f;
  ae->debounce_frames = debounce_frames;
  ae->frames_since_event = debounce_frames; // Ready to detect
  ae->event_active = false;
}

/**
 * @brief Process frame for event detection
 * @return true if new event detected (rising edge)
 */
static inline bool eif_audio_event_process(eif_audio_event_t *ae,
                                           float frame_energy) {
  ae->frames_since_event++;

  // Update baseline during quiet periods
  if (!ae->event_active && frame_energy < ae->threshold * 2.0f) {
    ae->baseline = (1.0f - ae->alpha) * ae->baseline + ae->alpha * frame_energy;
  }

  float relative_energy = frame_energy / (ae->baseline + 1e-6f);
  bool is_event = relative_energy > ae->threshold;

  // Rising edge detection with debounce
  bool new_event = false;
  if (is_event && !ae->event_active &&
      ae->frames_since_event >= ae->debounce_frames) {
    new_event = true;
    ae->frames_since_event = 0;
  }

  ae->event_active = is_event;

  return new_event;
}

// =============================================================================
// Zero-Crossing Rate (ZCR) for audio classification
// =============================================================================

/**
 * @brief Calculate zero-crossing rate
 */
static inline float eif_audio_zcr(const int16_t *samples, int num_samples) {
  int crossings = 0;

  for (int i = 1; i < num_samples; i++) {
    if ((samples[i - 1] >= 0 && samples[i] < 0) ||
        (samples[i - 1] < 0 && samples[i] >= 0)) {
      crossings++;
    }
  }

  return (float)crossings / num_samples;
}

/**
 * @brief Calculate spectral centroid approximation (using ZCR)
 */
static inline float eif_audio_spectral_centroid_approx(const int16_t *samples,
                                                       int num_samples) {
  // ZCR correlates with spectral centroid
  float zcr = eif_audio_zcr(samples, num_samples);

  // Approximate centroid from ZCR (assuming 16kHz sample rate)
  // This is a rough approximation
  return zcr * 8000.0f; // Hz
}

#ifdef __cplusplus
}
#endif

#endif // EIF_AUDIO_INTELLIGENCE_H
