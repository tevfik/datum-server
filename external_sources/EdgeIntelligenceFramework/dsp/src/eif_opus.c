#include "eif_opus.h"
#include <stdlib.h>
#include <string.h>

#ifdef EIF_USE_OPUS
#include "opus.h"
#endif

eif_status_t eif_opus_init(eif_opus_t *ctx, int sample_rate, int channels,
                           eif_opus_app_t app) {
  if (!ctx)
    return EIF_STATUS_ERROR;

#ifdef EIF_USE_OPUS
  ctx->sample_rate = sample_rate;
  ctx->channels = channels;
  ctx->application = (int)app;

  int err;
  ctx->encoder =
      opus_encoder_create(sample_rate, channels, ctx->application, &err);
  if (err != OPUS_OK || !ctx->encoder) {
    return EIF_STATUS_ERROR;
  }

  ctx->decoder = opus_decoder_create(sample_rate, channels, &err);
  if (err != OPUS_OK || !ctx->decoder) {
    opus_encoder_destroy(ctx->encoder);
    ctx->encoder = NULL;
    return EIF_STATUS_ERROR;
  }

  ctx->initialized = true;
  return EIF_STATUS_OK;
#else
  return EIF_STATUS_NOT_SUPPORTED;
#endif
}

int eif_opus_encode(eif_opus_t *ctx, const int16_t *pcm, int frame_size,
                    uint8_t *out, int max_bytes) {
#ifdef EIF_USE_OPUS
  if (!ctx || !ctx->initialized)
    return -1;
  return opus_encode(ctx->encoder, pcm, frame_size, out, max_bytes);
#else
  return -1;
#endif
}

int eif_opus_decode(eif_opus_t *ctx, const uint8_t *data, int len, int16_t *out,
                    int frame_size) {
#ifdef EIF_USE_OPUS
  if (!ctx || !ctx->initialized)
    return -1;
  // Decode Normal (FEC = 0)
  return opus_decode(ctx->decoder, data, len, out, frame_size, 0);
#else
  return -1;
#endif
}

void eif_opus_destroy(eif_opus_t *ctx) {
#ifdef EIF_USE_OPUS
  if (ctx) {
    if (ctx->encoder)
      opus_encoder_destroy(ctx->encoder);
    if (ctx->decoder)
      opus_decoder_destroy(ctx->decoder);
    ctx->encoder = NULL;
    ctx->decoder = NULL;
    ctx->initialized = false;
  }
#endif
}

eif_status_t eif_opus_set_complexity(eif_opus_t *ctx, int complexity) {
#ifdef EIF_USE_OPUS
  if (!ctx || !ctx->initialized)
    return EIF_STATUS_ERROR;
  if (complexity < 0)
    complexity = 0;
  if (complexity > 10)
    complexity = 10;

  int ret =
      opus_encoder_ctl(ctx->encoder, OPUS_SET_COMPLEXITY_REQUEST, complexity);
  return (ret == OPUS_OK) ? EIF_STATUS_OK : EIF_STATUS_ERROR;
#else
  return EIF_STATUS_NOT_SUPPORTED;
#endif
}
