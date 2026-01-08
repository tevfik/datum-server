/**
 * @file eif_opus.h
 * @brief EIF Wrapper for Opus Audio Codec
 *
 * Simplifies Opus usage for Edge Intelligence applications.
 */

#ifndef EIF_OPUS_H
#define EIF_OPUS_H

#include "eif_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration to avoid exposing Opus internals globally
typedef struct OpusEncoder OpusEncoder;
typedef struct OpusDecoder OpusDecoder;

typedef struct {
  OpusEncoder *encoder;
  OpusDecoder *decoder;
  int sample_rate;
  int channels;
  int application; // OPUS_APPLICATION_VOIP, etc.
  bool initialized;
} eif_opus_t;

// EIF Opus Application Types (Map to generic, not opus specific macros)
typedef enum {
  EIF_OPUS_APP_VOIP = 2048,
  EIF_OPUS_APP_AUDIO = 2049,
  EIF_OPUS_APP_LOWDELAY = 2051
} eif_opus_app_t;

/**
 * @brief Initialize Opus Wrapper
 * @param ctx Pointer to context
 * @param sample_rate 8000, 12000, 16000, 24000, or 48000
 * @param channels 1 or 2
 * @param app Application usage (VOIP/Audio)
 */
eif_status_t eif_opus_init(eif_opus_t *ctx, int sample_rate, int channels,
                           eif_opus_app_t app);

/**
 * @brief Encode PCM data to Opus packets
 * @param ctx Context
 * @param pcm Input PCM (interleaved if stereo)
 * @param frame_size Number of samples per channel (2.5, 5, 10, 20, 40, 60ms)
 * @param out Output buffer
 * @param max_bytes Output buffer size
 * @return Number of bytes encoded, or negative on error
 */
int eif_opus_encode(eif_opus_t *ctx, const int16_t *pcm, int frame_size,
                    uint8_t *out, int max_bytes);

/**
 * @brief Decode Opus packet to PCM
 * @param ctx Context
 * @param data Input Opus packet
 * @param len Length of Opus packet
 * @param out Output PCM buffer
 * @param frame_size Max frame size expected
 * @return Number of decoded samples per channel, or negative on error
 */
int eif_opus_decode(eif_opus_t *ctx, const uint8_t *data, int len, int16_t *out,
                    int frame_size);

/**
 * @brief Destroy Opus instance
 */
void eif_opus_destroy(eif_opus_t *ctx);

/**
 * @brief Set Encoder Complexity (0-10)
 * Lower = less CPU, lower quality
 */
eif_status_t eif_opus_set_complexity(eif_opus_t *ctx, int complexity);

#ifdef __cplusplus
}
#endif

#endif // EIF_OPUS_H
