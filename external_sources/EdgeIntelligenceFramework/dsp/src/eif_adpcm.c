#include "eif_adpcm.h"

/* IMA ADPCM Step Table */
static const int16_t step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15297, 16824, 18504, 20350, 22385, 24623, 27086, 29794, 32767
};

/* IMA ADPCM Index Adjustment Table */
static const int8_t index_adjust[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

void eif_adpcm_init(eif_adpcm_state_t *state) {
    if (state) {
        state->prev_sample = 0;
        state->step_index = 0;
    }
}

static uint8_t encode_sample(eif_adpcm_state_t *state, int16_t sample) {
    int diff = sample - state->prev_sample;
    int step = step_table[state->step_index];
    int mask = 8;
    int delta = 0;

    if (diff < 0) {
        diff = -diff;
        delta |= 8;
    }

    /* Quantize */
    if (diff >= step) {
        delta |= 4;
        diff -= step;
    }
    step >>= 1;
    if (diff >= step) {
        delta |= 2;
        diff -= step;
    }
    step >>= 1;
    if (diff >= step) {
        delta |= 1;
    }

    /* Inverse Quantize (Update predicted sample) to match decoder */
    diff = 0;
    int update_step = step_table[state->step_index];
    if (delta & 4) diff += update_step;
    if (delta & 2) diff += update_step >> 1;
    if (delta & 1) diff += update_step >> 2;
    diff += update_step >> 3;

    if (delta & 8) state->prev_sample -= diff;
    else state->prev_sample += diff;

    /* Clamp Output */
    if (state->prev_sample > 32767) state->prev_sample = 32767;
    else if (state->prev_sample < -32768) state->prev_sample = -32768;

    /* Update Index */
    state->step_index += index_adjust[delta & 7];
    if (state->step_index < 0) state->step_index = 0;
    else if (state->step_index > 88) state->step_index = 88;

    return (uint8_t)(delta & 0x0F);
}

size_t eif_adpcm_encode(eif_adpcm_state_t *state, const int16_t *pcm_in, uint8_t *adpcm_out, size_t len) {
    if (!state || !pcm_in || !adpcm_out) return 0;

    size_t i;
    for (i = 0; i < len; i += 2) {
        // High nibble first or low nibble first depends on format. 
        // Standard IMA is usually low nibble first but packed sequentially. 
        // Here we pack two samples into one byte: (Sample 2 << 4) | Sample 1
        
        uint8_t s1 = encode_sample(state, pcm_in[i]);
        uint8_t s2 = (i + 1 < len) ? encode_sample(state, pcm_in[i + 1]) : 0;
        
        adpcm_out[i / 2] = (uint8_t)((s2 << 4) | s1);
    }
    return (len + 1) / 2;
}

static int16_t decode_sample(eif_adpcm_state_t *state, uint8_t code) {
    int step = step_table[state->step_index];
    int diff = step >> 3;

    if (code & 4) diff += step;
    if (code & 2) diff += step >> 1;
    if (code & 1) diff += step >> 2;

    if (code & 8) state->prev_sample -= diff;
    else state->prev_sample += diff;

    /* Clamp Output */
    if (state->prev_sample > 32767) state->prev_sample = 32767;
    else if (state->prev_sample < -32768) state->prev_sample = -32768;

    /* Update Index */
    state->step_index += index_adjust[code & 7];
    if (state->step_index < 0) state->step_index = 0;
    else if (state->step_index > 88) state->step_index = 88;

    return state->prev_sample;
}

size_t eif_adpcm_decode(eif_adpcm_state_t *state, const uint8_t *adpcm_in, int16_t *pcm_out, size_t len) {
    if (!state || !adpcm_in || !pcm_out) return 0;
    
    size_t i;
    for (i = 0; i < len; i++) {
        uint8_t byte = adpcm_in[i];
        
        pcm_out[i * 2] = decode_sample(state, byte & 0x0F);
        pcm_out[i * 2 + 1] = decode_sample(state, (byte >> 4) & 0x0F);
    }
    return len * 2;
}
