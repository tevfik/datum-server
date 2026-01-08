#include "eif_dsp.h"
#include <immintrin.h>
#include <string.h>

// Check for AVX2 support
#if defined(__AVX2__)
    #define EIF_USE_AVX2
#endif

void eif_fft_process_stage_simd(int n, int size, int half_size, int table_step, 
                                float32_t* data, const float32_t* twiddles, bool inverse) {
#if defined(EIF_USE_AVX2)
    // We process 4 complex pairs (8 floats) at a time.
    // So we need half_size >= 4.
    if (half_size < 4) return; // Fallback to scalar caller

    // If table_step is 1, twiddles are contiguous (complex).
    // If table_step > 1, we need to gather.
    // Let's optimize for table_step == 1 (Last stage) first, as it's the largest loop.
    // And maybe table_step == 2, 4 if easy.
    // Actually, let's try to handle general case with gather if efficient, or just fallback.
    
    // For now, only optimize if table_step == 1 (Last stage).
    // This covers 50% of the work? No, last stage has N/2 butterflies. Total is N/2 * logN.
    // So it's 1/logN of the work. Not great.
    
    // We should handle any step.
    // But gathering complex numbers is tricky.
    // Let's just implement for table_step == 1 for now to prove concept.
    
    if (table_step == 1) {
        for (int i = 0; i < n; i += size) {
            int j = 0;
            for (; j <= half_size - 4; j += 4) {
                // Indices
                int even_idx = 2 * (i + j);
                int odd_idx = 2 * (i + j + half_size);
                int k = j; // table_step = 1
                
                // Load Data
                // even_r/i for 4 points: [e0r, e0i, e1r, e1i, e2r, e2i, e3r, e3i]
                __m256 even_vec = _mm256_loadu_ps(&data[even_idx]);
                __m256 odd_vec = _mm256_loadu_ps(&data[odd_idx]);
                
                // Load Twiddles
                // [w0r, w0i, w1r, w1i, w2r, w2i, w3r, w3i]
                __m256 w_vec = _mm256_loadu_ps(&twiddles[2*k]);
                
                if (inverse) {
                    // Conjugate w: negate imaginary parts.
                    // Mask: [0, -0, 0, -0, ...] (0x80000000)
                    // Or just multiply by [1, -1, 1, -1...]
                    __m256 mask = _mm256_setr_ps(1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f);
                    w_vec = _mm256_mul_ps(w_vec, mask);
                }
                
                // Complex Multiply: (odd_r + j*odd_i) * (w_r + j*w_i)
                // = (odd_r*w_r - odd_i*w_i) + j(odd_r*w_i + odd_i*w_r)
                
                // AVX2 doesn't have native complex mul.
                // We need to shuffle.
                
                // odd_vec: [r0, i0, r1, i1, r2, i2, r3, i3]
                // w_vec:   [wr0, wi0, wr1, wi1, wr2, wi2, wr3, wi3]
                
                // 1. odd_r * w_r
                // 2. odd_i * w_i
                // 3. odd_r * w_i
                // 4. odd_i * w_r
                
                // Permute odd to get [r0, r0, r1, r1...] and [i0, i0, i1, i1...]
                __m256 odd_r = _mm256_movehdup_ps(odd_vec); // [r0, r0, r1, r1...] ? No, movehdup duplicates odd positions (1,3..). i.e. Imag.
                // wait, movehdup: dest[0] = src[1], dest[1] = src[1].
                // moveldup: dest[0] = src[0], dest[1] = src[0].
                
                __m256 odd_real_dup = _mm256_moveldup_ps(odd_vec); // [r0, r0, r1, r1...]
                __m256 odd_imag_dup = _mm256_movehdup_ps(odd_vec); // [i0, i0, i1, i1...]
                
                // We want:
                // Real part result: r*wr - i*wi
                // Imag part result: r*wi + i*wr
                
                // w_vec is [wr, wi, wr, wi...]
                // Multiply odd_real_dup * w_vec -> [r*wr, r*wi, r*wr, r*wi...]
                __m256 t1 = _mm256_mul_ps(odd_real_dup, w_vec);
                
                // Swap w_vec real/imag: [wi, wr, wi, wr...]
                __m256 w_swapped = _mm256_permute_ps(w_vec, 0xB1); // 10 11 00 01 -> 2 3 0 1? No.
                // _MM_SHUFFLE(2, 3, 0, 1) swaps adjacent pairs.
                
                // Multiply odd_imag_dup * w_swapped -> [i*wi, i*wr, i*wi, i*wr...]
                __m256 t2 = _mm256_mul_ps(odd_imag_dup, w_swapped);
                
                // Now we need add/sub.
                // Real: t1_real - t2_real (r*wr - i*wi)
                // Imag: t1_imag + t2_imag (r*wi + i*wr)
                
                // t1: [r*wr, r*wi, ...]
                // t2: [i*wi, i*wr, ...]
                
                // We want [r*wr - i*wi, r*wi + i*wr, ...]
                // This is addsub instruction? _mm256_addsub_ps does (a - b) for odd, (a + b) for even indices?
                // Intel docs: "Subtracts odd ... adds even".
                // Wait, index 0 is even. index 1 is odd.
                // dest[0] = a[0] - b[0]
                // dest[1] = a[1] + b[1]
                
                // We want Real (index 0) to be SUB. Imag (index 1) to be ADD.
                // So addsub matches!
                
                __m256 twiddled = _mm256_addsub_ps(t1, t2);
                // Wait, check order.
                // t1[0] = r*wr. t2[0] = i*wi. Result[0] = r*wr - i*wi. Correct.
                // t1[1] = r*wi. t2[1] = i*wr. Result[1] = r*wi + i*wr. Correct.
                
                // Butterfly:
                // even' = even + twiddled
                // odd'  = even - twiddled
                
                __m256 new_even = _mm256_add_ps(even_vec, twiddled);
                __m256 new_odd  = _mm256_sub_ps(even_vec, twiddled);
                
                _mm256_storeu_ps(&data[even_idx], new_even);
                _mm256_storeu_ps(&data[odd_idx], new_odd);
            }
        }
    }
#endif
}
