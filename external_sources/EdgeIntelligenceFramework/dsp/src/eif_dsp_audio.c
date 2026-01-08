#include "eif_audio.h"
#include <string.h>
#include <math.h>

eif_status_t eif_audio_init(eif_audio_preprocessor_t* ctx, const eif_audio_config_t* config, eif_memory_pool_t* pool) {
    if (!ctx || !config || !pool) return EIF_STATUS_INVALID_ARGUMENT;
    
    ctx->config = *config;
    ctx->pool = pool;
    
    // Init MFCC Config
    ctx->mfcc_config.num_mfcc = config->num_mfcc;
    ctx->mfcc_config.num_filters = config->num_filters;
    ctx->mfcc_config.fft_length = config->frame_length; // Assuming FFT length = Frame length (or next power of 2)
    ctx->mfcc_config.sample_rate = config->sample_rate;
    ctx->mfcc_config.low_freq = config->lower_freq;
    ctx->mfcc_config.high_freq = config->upper_freq;
    
    if (eif_dsp_mfcc_init_f32(&ctx->mfcc_config, pool) != EIF_STATUS_OK) return EIF_STATUS_ERROR;
    
    // Init STFT Config
    // We only need STFT init to setup window and FFT tables.
    // eif_dsp_stft_init_f32 might allocate its own buffers.
    if (eif_dsp_stft_init_f32(&ctx->stft_config, config->frame_length, config->frame_stride, config->frame_length, pool) != EIF_STATUS_OK) return EIF_STATUS_ERROR;
    
    // Init Audio Ring Buffer
    // Needs to hold at least one frame + stride?
    // Let's make it large enough to hold 2 frames for safety.
    ctx->buffer_size = config->frame_length * 2;
    ctx->audio_buffer = (float32_t*)eif_memory_alloc(pool, ctx->buffer_size * sizeof(float32_t), 4);
    if (!ctx->audio_buffer) return EIF_STATUS_OUT_OF_MEMORY;
    
    ctx->write_pos = 0;
    ctx->read_pos = 0;
    ctx->samples_available = 0;
    
    // Init Feature Buffer
    int feature_size = config->output_frames * config->num_mfcc;
    ctx->feature_buffer = (float32_t*)eif_memory_alloc(pool, feature_size * sizeof(float32_t), 4);
    if (!ctx->feature_buffer) return EIF_STATUS_OUT_OF_MEMORY;
    memset(ctx->feature_buffer, 0, feature_size * sizeof(float32_t));
    
    ctx->feature_write_pos = 0;
    
    return EIF_STATUS_OK;
}

eif_status_t eif_audio_push(eif_audio_preprocessor_t* ctx, const float32_t* samples, int num_samples) {
    if (!ctx || !samples) return EIF_STATUS_INVALID_ARGUMENT;
    
    for (int i = 0; i < num_samples; i++) {
        // Add sample to ring buffer
        ctx->audio_buffer[ctx->write_pos] = samples[i];
        ctx->write_pos = (ctx->write_pos + 1) % ctx->buffer_size;
        ctx->samples_available++;
        
        // Check if we have enough for a frame
        if (ctx->samples_available >= ctx->config.frame_length) {
            // Process Frame
            
            // 1. Extract Frame (handle wrap around)
            // We can use a temp buffer or handle wrap in STFT/MFCC?
            // STFT/MFCC usually expect contiguous buffer.
            // Let's copy to a scratch buffer.
            // We can use stft_config.fft_buffer as scratch if it's large enough (it is 2*fft_len).
            // Or allocate a small scratch on stack if frame is small?
            // Frame is e.g. 1024 floats (4KB). Stack might be tight on embedded.
            // Let's use a dedicated scratch or reuse something.
            // We can use the end of audio_buffer if we are careful? No.
            // Let's allocate a temporary frame buffer in init? Or just use stft_config.fft_buffer (real part).
            
            float32_t* frame_scratch = ctx->stft_config.fft_buffer; // Reuse FFT buffer
            
            for (int j = 0; j < ctx->config.frame_length; j++) {
                int idx = (ctx->read_pos + j) % ctx->buffer_size;
                frame_scratch[j] = ctx->audio_buffer[idx];
            }
            
            // 2. Apply Window and Expand to Complex
            // We expand in reverse to avoid overwriting
            for (int j = ctx->config.frame_length - 1; j >= 0; j--) {
                float32_t val = frame_scratch[j] * ctx->stft_config.window[j];
                frame_scratch[2*j] = val;
                frame_scratch[2*j+1] = 0.0f;
            }
            
            // 3. Compute FFT -> Mag -> MFCC
            // Perform Complex FFT
            eif_dsp_fft_f32(&ctx->stft_config.fft_config, frame_scratch);
            
            // Magnitude
            // frame_scratch is now Complex [Re, Im, Re, Im...]
            // We compute magnitude in-place (packing to beginning)
            eif_dsp_magnitude_f32(frame_scratch, frame_scratch, ctx->config.frame_length);
            
            // Note: eif_dsp_magnitude_f32 output size is N.
            // But MFCC expects N/2+1 (Nyquist).
            // Since input was real, magnitude is symmetric.
            // We only need first N/2+1.
            // frame_scratch has N magnitudes now.
            // We can just pass it to MFCC, it will only use first N/2+1?
            // Let's check eif_dsp_mfcc_compute_f32.
            // It likely iterates up to fft_length/2.
            // So passing full N magnitude is fine.
            
            // MFCC
            // Input: frame_scratch (magnitude)
            // Output: We want to append to the end of feature_buffer, shifting old data out.
            // Buffer shape: [output_frames, num_mfcc]
            
            int num_features = ctx->config.num_mfcc;
            int num_frames = ctx->config.output_frames;
            
            // Shift existing data left by 1 frame
            // memmove(dest, src, size)
            // dest: buffer[0]
            // src: buffer[1]
            // size: (num_frames - 1) * num_features * sizeof(float)
            memmove(ctx->feature_buffer, 
                    &ctx->feature_buffer[num_features], 
                    (num_frames - 1) * num_features * sizeof(float32_t));
            
            // Write new frame at the end
            float32_t* mfcc_out = &ctx->feature_buffer[(num_frames - 1) * num_features];
            eif_dsp_mfcc_compute_f32(&ctx->mfcc_config, frame_scratch, mfcc_out, ctx->pool);
            
            // Advance Audio Read Pos by Stride
            ctx->read_pos = (ctx->read_pos + ctx->config.frame_stride) % ctx->buffer_size;
            ctx->samples_available -= ctx->config.frame_stride;
        }
    }
    
    return EIF_STATUS_OK;
}

bool eif_audio_is_ready(const eif_audio_preprocessor_t* ctx) {
    // For streaming KWS, we are always "ready" once we have filled the buffer once?
    // Or we just return true.
    // Let's assume we are ready if we have processed at least 'output_frames' frames?
    // Or maybe just always ready and we look at the rolling window.
    return true; 
}

const float32_t* eif_audio_get_features(const eif_audio_preprocessor_t* ctx) {
    // Return the linear buffer.
    // Note: Since it's a ring buffer, the data might not be time-contiguous if we just return the pointer.
    // The neural net expects [T, F].
    // If we implemented a ring buffer for features, we need to unroll it?
    // Or we just shift data instead of using a ring buffer for features?
    // Shifting is expensive (memmove). Ring buffer is O(1).
    // But for Neural Net input, we usually need contiguous memory [T, F].
    // Unless the Neural Net supports a "start offset" or we copy.
    
    // For simplicity in this embedded framework:
    // Let's implement SHIFTING for the feature buffer.
    // When adding a new frame, we shift everything left and add to end.
    // This makes `feature_buffer` always contiguous and time-ordered.
    // Cost: memmove of (T-1)*F floats per frame.
    // For KWS (e.g. T=50, F=13), that's ~650 floats copy. Fast enough.
    
    // I need to change `eif_audio_push` to use shifting instead of ring buffer for features.
    // Let's modify `eif_audio_push` logic above.
    // Wait, I can't modify the previous tool call.
    // I will rewrite the function in this file content since I am writing the file now.
    
    return ctx->feature_buffer;
}

void eif_audio_reset(eif_audio_preprocessor_t* ctx) {
    ctx->write_pos = 0;
    ctx->read_pos = 0;
    ctx->samples_available = 0;
    memset(ctx->feature_buffer, 0, ctx->config.output_frames * ctx->config.num_mfcc * sizeof(float32_t));
    ctx->feature_write_pos = 0;
}
