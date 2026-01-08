#include "opus.h"
#include <stdlib.h>
#include <string.h>

// Mock Implementation for linking verification

int opus_encoder_get_size(int channels) { return 1024 * channels; } // Mock size
OpusEncoder *opus_encoder_create(int Fs, int channels, int application,
                                 int *error) {
  if (error)
    *error = OPUS_OK;
  return (OpusEncoder *)malloc(opus_encoder_get_size(channels));
}
int opus_encoder_init(OpusEncoder *st, int Fs, int channels, int application) {
  return OPUS_OK;
}
int opus_encode(OpusEncoder *st, const short *pcm, int frame_size,
                unsigned char *data, int max_data_bytes) {
  // Mock encode: just copy input bytes effectively? No, compression.
  // Just return a dummy length.
  if (max_data_bytes > 10) {
    memset(data, 0xAA, 10);
    return 10;
  }
  return -1;
}
void opus_encoder_destroy(OpusEncoder *st) { free(st); }

int opus_decoder_get_size(int channels) { return 1024 * channels; }
OpusDecoder *opus_decoder_create(int Fs, int channels, int *error) {
  if (error)
    *error = OPUS_OK;
  return (OpusDecoder *)malloc(opus_decoder_get_size(channels));
}
int opus_decoder_init(OpusDecoder *st, int Fs, int channels) { return OPUS_OK; }
int opus_decode(OpusDecoder *st, const unsigned char *data, int len, short *pcm,
                int frame_size, int decode_fec) {
  // Mock decode: generate silence or pattern
  // memset(pcm, 0, frame_size * sizeof(short)); // Silence
  // Let's generate a pattern to verify interaction
  for (int i = 0; i < frame_size; i++)
    pcm[i] = (short)(i & 0xFFFF);
  return frame_size;
}
void opus_decoder_destroy(OpusDecoder *st) { free(st); }
