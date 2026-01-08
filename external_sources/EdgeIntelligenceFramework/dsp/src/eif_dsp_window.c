#include "eif_dsp.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void eif_dsp_window_hamming_f32(float32_t* window, size_t length) {
    for (size_t i = 0; i < length; i++) {
        window[i] = 0.54f - 0.46f * cosf(2 * M_PI * i / (length - 1));
    }
}

void eif_dsp_window_hanning_f32(float32_t* window, size_t length) {
    for (size_t i = 0; i < length; i++) {
        window[i] = 0.5f * (1.0f - cosf(2 * M_PI * i / (length - 1)));
    }
}
