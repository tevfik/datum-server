#ifndef OPUS_H
#define OPUS_H

#ifdef __cplusplus
extern "C" {
#endif

// Mocked definitions for verification
#define OPUS_OK 0
#define OPUS_BAD_ARG -1
#define OPUS_APPLICATION_VOIP 2048
#define OPUS_APPLICATION_AUDIO 2049
#define OPUS_APPLICATION_RESTRICTED_LOWDELAY 2051

// Requests
#define OPUS_SET_BITRATE_REQUEST 4002
#define OPUS_GET_BITRATE_REQUEST 4003
#define OPUS_SET_COMPLEXITY_REQUEST 4010
#define OPUS_GET_COMPLEXITY_REQUEST 4011

// Variadic macros for mocking
#define opus_encoder_ctl(st, request, ...) (OPUS_OK)
#define opus_decoder_ctl(st, request, ...) (OPUS_OK)

typedef struct OpusEncoder OpusEncoder;
typedef struct OpusDecoder OpusDecoder;

int opus_encoder_get_size(int channels);
OpusEncoder *opus_encoder_create(int Fs, int channels, int application,
                                 int *error);
int opus_encoder_init(OpusEncoder *st, int Fs, int channels, int application);
int opus_encode(OpusEncoder *st, const short *pcm, int frame_size,
                unsigned char *data, int max_data_bytes);
void opus_encoder_destroy(OpusEncoder *st);

int opus_decoder_get_size(int channels);
OpusDecoder *opus_decoder_create(int Fs, int channels, int *error);
int opus_decoder_init(OpusDecoder *st, int Fs, int channels);
int opus_decode(OpusDecoder *st, const unsigned char *data, int len, short *pcm,
                int frame_size, int decode_fec);
void opus_decoder_destroy(OpusDecoder *st);

#ifdef __cplusplus
}
#endif

#endif // OPUS_H
