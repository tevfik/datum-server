#ifndef EIF_DSP_FIXED_H
#define EIF_DSP_FIXED_H

#include "eif_fixedpoint.h"
#include "eif_status.h"
#include "eif_memory.h"
#include <stdbool.h>
#include <stddef.h>

// Fixed-Point FFT Configuration (Q15)
typedef struct {
    uint16_t length;
    bool inverse;
    uint16_t* bit_reverse_indices; 
    q15_t* twiddle_factors;   // Precomputed twiddle factors (2*length Q15: cos, sin)
} eif_fft_config_q15_t;

// Initialize Q15 FFT Context
eif_status_t eif_dsp_fft_init_q15(eif_fft_config_q15_t* config, uint16_t length, eif_memory_pool_t* pool);
eif_status_t eif_dsp_fft_deinit_q15(eif_fft_config_q15_t* config);

// Perform Q15 FFT (Complex)
// Input: Real/Imag interleaved [r0, i0, r1, i1, ...]
// Note: Performs scaling by >>1 at each stage to prevent overflow.
eif_status_t eif_dsp_fft_q15(const eif_fft_config_q15_t* config, q15_t* data);

// Perform Q15 Real FFT (RFFT)
// Input: Real array [x0, x1, ... xN-1] (Length N)
// Output: Complex array [r0, i0, r1, i1, ... r(N/2), i(N/2)] (Length N+2 Q15s)
eif_status_t eif_dsp_rfft_q15(const eif_fft_config_q15_t* config, const q15_t* input_real, q15_t* output_complex, eif_memory_pool_t* pool);

// Compute Magnitude Spectrum Q15
// Input: Complex array [r0, i0, ... ]
// Output: Magnitude array [m0, m1, ... ]
eif_status_t eif_dsp_magnitude_q15(const q15_t* input_complex, q15_t* output_mag, size_t length);

// --- Windowing Functions ---
// Generate Window Coefficients (Q15)
// Output: window array of size 'length'
eif_status_t eif_dsp_window_hamming_q15(q15_t* window, size_t length);
eif_status_t eif_dsp_window_hanning_q15(q15_t* window, size_t length);

eif_status_t eif_dsp_window_hanning_q15(q15_t* window, size_t length);

// --- Filters ---
// FIR Filter
// Coeffs and Input are Q15. Accumulation in Q31/Q63. Output Q15.
eif_status_t eif_dsp_fir_q15(const q15_t* input, q15_t* output, size_t length, const q15_t* coeffs, size_t taps);

// IIR Filter (Biquad)
// Coeffs: [b0, b1, b2, a1, a2] per stage (Q15)
// State: [x[n-1], x[n-2], y[n-1], y[n-2]] per stage (Q15)
// Note: Coefficients must be scaled to fit Q15 (usually divided by 2 or more if gain > 1).
// Standard Biquad implementation often assumes coeffs in Q14 or similar. We stick to Q15.
eif_status_t eif_dsp_iir_q15(const q15_t* input, q15_t* output, size_t length, 
                             const q15_t* coeffs, q15_t* state, size_t stages);

// --- Statistics ---
// Root Mean Square (RMS)
q15_t eif_dsp_rms_q15(const q15_t* input, size_t length);

// Zero Crossing Rate (ZCR)
// Returns rate in Q15 (0.0 to 1.0)
q15_t eif_dsp_zcr_q15(const q15_t* input, size_t length);

// --- MFCC (Mel-Frequency Cepstral Coefficients) ---
typedef struct {
    int num_mfcc;       // Number of MFCC coefficients (e.g., 13)
    int num_filters;    // Number of Mel filters (e.g., 26)
    int fft_length;     // FFT Length
    int sample_rate;    // Sample Rate
    int low_freq;       // Low frequency cutoff
    int high_freq;      // High frequency cutoff
    
    // Internal buffers (Q15)
    // Filterbank: Flattened [num_filters * (fft_length/2 + 1)]
    q15_t* filter_bank; 
    // DCT Matrix: Flattened [num_mfcc * num_filters]
    q15_t* dct_matrix;
} eif_mfcc_config_q15_t;

// Initialize MFCC (Precompute Tables)
// Uses memory pool for temporary calculations if needed, but tables are allocated in config pointers (user must allocate them or we use a pool?)
// Let's assume user provides allocated buffers in config->filter_bank and config->dct_matrix before calling init?
// Or we pass a pool to allocate them?
// Let's stick to the pattern: Init computes values into provided buffers.
eif_status_t eif_dsp_mfcc_init_q15(eif_mfcc_config_q15_t* config);

// Compute MFCC
// Input: FFT Magnitude Spectrum (Q15) [fft_length/2 + 1]
// Output: MFCC Coefficients (Q15) [num_mfcc]
// Uses pool for intermediate buffers (Mel energies, Log energies)
eif_status_t eif_dsp_mfcc_compute_q15(const eif_mfcc_config_q15_t* config, const q15_t* fft_mag, q15_t* mfcc_out, eif_memory_pool_t* pool);

#endif // EIF_DSP_FIXED_H
