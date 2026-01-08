#ifndef EIF_ADPCM_H
#define EIF_ADPCM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// IMA ADPCM State
typedef struct {
    int16_t prev_sample; // Previous sample value
    int8_t  step_index;  // Index into step size table
} eif_adpcm_state_t;

/**
 * @brief Initialize ADPCM state
 * @param state Pointer to state structure
 */
void eif_adpcm_init(eif_adpcm_state_t *state);

/**
 * @brief Encode a buffer of 16-bit PCM samples to 4-bit IMA ADPCM
 * 
 * @param state Current ADPCM state (updated in place)
 * @param pcm_in Input buffer (16-bit PCM)
 * @param adpcm_out Output buffer (packed 4-bit ADPCM, size must be len/2)
 * @param len Number of samples to encode
 * @return Number of bytes written to output buffer
 */
size_t eif_adpcm_encode(eif_adpcm_state_t *state, const int16_t *pcm_in, uint8_t *adpcm_out, size_t len);

/**
 * @brief Decode a buffer of 4-bit IMA ADPCM to 16-bit PCM
 * 
 * @param state Current ADPCM state (updated in place)
 * @param adpcm_in Input buffer (packed 4-bit ADPCM)
 * @param pcm_out Output buffer (16-bit PCM, size must be len*2)
 * @param len Number of ADPCM bytes to decode (produces len*2 samples)
 * @return Number of samples written to output buffer
 */
size_t eif_adpcm_decode(eif_adpcm_state_t *state, const uint8_t *adpcm_in, int16_t *pcm_out, size_t len);

#ifdef __cplusplus
}
#endif

#endif // EIF_ADPCM_H
