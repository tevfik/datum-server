/**
 * @file eif_wake_word.h
 * @brief Wake Word Detection for Edge AI
 *
 * Lightweight keyword spotting support:
 * - MFCC feature extraction
 * - Sliding window processing
 * - Template matching
 *
 * Designed for always-on voice interfaces on MCUs.
 */

#ifndef EIF_WAKE_WORD_H
#define EIF_WAKE_WORD_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EIF_WW_FRAME_SIZE 256 // 16ms at 16kHz
#define EIF_WW_HOP_SIZE 128   // 8ms hop
#define EIF_WW_NUM_MFCC 13
#define EIF_WW_MAX_FRAMES 50 // ~400ms max keyword
#define EIF_WW_MAX_KEYWORDS 5

// =============================================================================
// MFCC Extraction (Simplified)
// =============================================================================

/**
 * @brief Simplified MFCC feature extraction
 * Uses a mel-scale approximation without full FFT for efficiency.
 */
typedef struct {
  float prev_energy;
  float delta[EIF_WW_NUM_MFCC];
} eif_mfcc_state_t;

/**
 * @brief Initialize MFCC state
 */
static inline void eif_mfcc_init(eif_mfcc_state_t *state) {
  state->prev_energy = 0;
  for (int i = 0; i < EIF_WW_NUM_MFCC; i++) {
    state->delta[i] = 0;
  }
}

/**
 * @brief Extract simplified MFCC-like features from audio frame
 *
 * This is a lightweight approximation for embedded use.
 * For full MFCC, use the DSP module's eif_mfcc functions.
 */
static inline void eif_mfcc_extract_simple(const int16_t *frame, int frame_size,
                                           float *mfcc, int num_coeffs) {
  // Pre-emphasis
  float pre_emph[EIF_WW_FRAME_SIZE];
  pre_emph[0] = frame[0];
  for (int i = 1; i < frame_size; i++) {
    pre_emph[i] = frame[i] - 0.97f * frame[i - 1];
  }

  // Compute energy in sub-bands (mel approximation)
  int bands = num_coeffs;
  int band_size = frame_size / bands;

  for (int b = 0; b < bands; b++) {
    float energy = 0;
    int start = b * band_size;

    // Apply triangular window
    for (int i = 0; i < band_size; i++) {
      float sample = pre_emph[start + i] / 32768.0f;
      float window = 1.0f - fabsf(2.0f * i / band_size - 1.0f);
      energy += sample * sample * window;
    }

    // Log energy (mel-like)
    mfcc[b] = logf(energy + 1e-10f);
  }

  // Simple DCT approximation (just normalize)
  float mean = 0;
  for (int i = 0; i < num_coeffs; i++) {
    mean += mfcc[i];
  }
  mean /= num_coeffs;

  for (int i = 0; i < num_coeffs; i++) {
    mfcc[i] -= mean;
  }
}

// =============================================================================
// Keyword Template
// =============================================================================

/**
 * @brief Keyword template for matching
 */
typedef struct {
  char name[16];
  float features[EIF_WW_MAX_FRAMES][EIF_WW_NUM_MFCC];
  int num_frames;
  float threshold;
} eif_keyword_template_t;

// =============================================================================
// Wake Word Detector
// =============================================================================

/**
 * @brief Wake word detector state
 */
typedef struct {
  // Audio buffer (ring buffer)
  int16_t audio_buffer[EIF_WW_FRAME_SIZE * 4];
  int audio_idx;
  int audio_count;

  // Feature buffer
  float features[EIF_WW_MAX_FRAMES][EIF_WW_NUM_MFCC];
  int feature_idx;
  int feature_count;

  // Keywords
  eif_keyword_template_t keywords[EIF_WW_MAX_KEYWORDS];
  int num_keywords;

  // Energy-based VAD
  float noise_floor;
  float energy_threshold;
  bool voice_active;
  int voice_frames;

  // State
  eif_mfcc_state_t mfcc_state;
  int samples_since_hop;
} eif_wake_word_t;

/**
 * @brief Initialize wake word detector
 */
static inline void eif_wake_word_init(eif_wake_word_t *ww,
                                      float energy_threshold) {
  ww->audio_idx = 0;
  ww->audio_count = 0;
  ww->feature_idx = 0;
  ww->feature_count = 0;
  ww->num_keywords = 0;
  ww->noise_floor = 0.001f;
  ww->energy_threshold = energy_threshold;
  ww->voice_active = false;
  ww->voice_frames = 0;
  ww->samples_since_hop = 0;
  eif_mfcc_init(&ww->mfcc_state);
}

/**
 * @brief Add keyword template
 */
static inline bool
eif_wake_word_add_keyword(eif_wake_word_t *ww, const char *name,
                          const float features[][EIF_WW_NUM_MFCC],
                          int num_frames, float threshold) {
  if (ww->num_keywords >= EIF_WW_MAX_KEYWORDS)
    return false;
  if (num_frames > EIF_WW_MAX_FRAMES)
    num_frames = EIF_WW_MAX_FRAMES;

  eif_keyword_template_t *kw = &ww->keywords[ww->num_keywords];

  // Copy name
  int i;
  for (i = 0; name[i] && i < 15; i++) {
    kw->name[i] = name[i];
  }
  kw->name[i] = '\0';

  // Copy features
  kw->num_frames = num_frames;
  kw->threshold = threshold;
  for (int f = 0; f < num_frames; f++) {
    for (int c = 0; c < EIF_WW_NUM_MFCC; c++) {
      kw->features[f][c] = features[f][c];
    }
  }

  ww->num_keywords++;
  return true;
}

/**
 * @brief Calculate frame energy
 */
static inline float eif_wake_word_frame_energy(const int16_t *frame, int size) {
  float energy = 0;
  for (int i = 0; i < size; i++) {
    float s = frame[i] / 32768.0f;
    energy += s * s;
  }
  return energy / size;
}

/**
 * @brief DTW matching for keyword
 */
static inline float eif_wake_word_match(const float input[][EIF_WW_NUM_MFCC],
                                        int input_len,
                                        const float tmpl[][EIF_WW_NUM_MFCC],
                                        int tmpl_len) {
  // Simplified DTW
  if (input_len < 5 || tmpl_len < 5)
    return 1e30f;

  float prev[EIF_WW_MAX_FRAMES + 1];
  float curr[EIF_WW_MAX_FRAMES + 1];

  for (int j = 0; j <= tmpl_len; j++)
    prev[j] = 1e30f;
  prev[0] = 0;

  for (int i = 1; i <= input_len; i++) {
    curr[0] = 1e30f;

    for (int j = 1; j <= tmpl_len; j++) {
      // Euclidean distance
      float dist = 0;
      for (int k = 0; k < EIF_WW_NUM_MFCC; k++) {
        float d = input[i - 1][k] - tmpl[j - 1][k];
        dist += d * d;
      }
      dist = sqrtf(dist);

      float min_prev = prev[j - 1];
      if (prev[j] < min_prev)
        min_prev = prev[j];
      if (curr[j - 1] < min_prev)
        min_prev = curr[j - 1];

      curr[j] = dist + min_prev;
    }

    for (int j = 0; j <= tmpl_len; j++)
      prev[j] = curr[j];
  }

  return prev[tmpl_len] / (input_len + tmpl_len);
}

/**
 * @brief Process audio samples
 * @return Index of detected keyword (0-based), or -1 if none
 */
static inline int eif_wake_word_process(eif_wake_word_t *ww,
                                        const int16_t *samples,
                                        int num_samples) {
  int detected = -1;

  for (int s = 0; s < num_samples; s++) {
    // Add to audio buffer
    ww->audio_buffer[ww->audio_idx] = samples[s];
    ww->audio_idx = (ww->audio_idx + 1) % (EIF_WW_FRAME_SIZE * 4);
    if (ww->audio_count < EIF_WW_FRAME_SIZE * 4)
      ww->audio_count++;
    ww->samples_since_hop++;

    // Process on hop boundary
    if (ww->samples_since_hop >= EIF_WW_HOP_SIZE &&
        ww->audio_count >= EIF_WW_FRAME_SIZE) {

      ww->samples_since_hop = 0;

      // Extract frame (handle wrap-around)
      int16_t frame[EIF_WW_FRAME_SIZE];
      int start = (ww->audio_idx - EIF_WW_FRAME_SIZE + EIF_WW_FRAME_SIZE * 4) %
                  (EIF_WW_FRAME_SIZE * 4);
      for (int i = 0; i < EIF_WW_FRAME_SIZE; i++) {
        frame[i] = ww->audio_buffer[(start + i) % (EIF_WW_FRAME_SIZE * 4)];
      }

      // Calculate energy
      float energy = eif_wake_word_frame_energy(frame, EIF_WW_FRAME_SIZE);

      // Update noise floor
      if (!ww->voice_active && energy < ww->energy_threshold) {
        ww->noise_floor = 0.99f * ww->noise_floor + 0.01f * energy;
      }

      // VAD
      bool is_voice = energy > ww->noise_floor * ww->energy_threshold;

      if (is_voice) {
        ww->voice_frames++;

        // Extract features
        if (ww->feature_count < EIF_WW_MAX_FRAMES) {
          eif_mfcc_extract_simple(frame, EIF_WW_FRAME_SIZE,
                                  ww->features[ww->feature_count],
                                  EIF_WW_NUM_MFCC);
          ww->feature_count++;
        }

        ww->voice_active = true;
      } else if (ww->voice_active) {
        ww->voice_frames++;

        // End of utterance
        if (ww->voice_frames > 5 && ww->feature_count > 10) {
          // Match against keywords
          float best_dist = 1e30f;
          int best_kw = -1;

          for (int k = 0; k < ww->num_keywords; k++) {
            eif_keyword_template_t *kw = &ww->keywords[k];
            float dist = eif_wake_word_match(
                (const float (*)[EIF_WW_NUM_MFCC])ww->features,
                ww->feature_count,
                (const float (*)[EIF_WW_NUM_MFCC])kw->features, kw->num_frames);

            if (dist < best_dist && dist < kw->threshold) {
              best_dist = dist;
              best_kw = k;
            }
          }

          detected = best_kw;
        }

        // Reset
        ww->voice_active = false;
        ww->voice_frames = 0;
        ww->feature_count = 0;
      }
    }
  }

  return detected;
}

/**
 * @brief Get keyword name by index
 */
static inline const char *eif_wake_word_get_name(eif_wake_word_t *ww,
                                                 int index) {
  if (index >= 0 && index < ww->num_keywords) {
    return ww->keywords[index].name;
  }
  return NULL;
}

#ifdef __cplusplus
}
#endif

#endif // EIF_WAKE_WORD_H
