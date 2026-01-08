#ifndef EIF_AUDIO_H
#define EIF_AUDIO_H

#include "eif_dsp.h"
#include "eif_memory.h"

// Audio Preprocessor Configuration
typedef struct {
    int sample_rate;
    int frame_length;       // e.g., 1024 (20-30ms)
    int frame_stride;       // e.g., 512 (50% overlap)
    int num_mfcc;           // e.g., 13 or 40 (if using Mel Spectrogram directly)
    int num_filters;        // e.g., 40
    float32_t lower_freq;
    float32_t upper_freq;
    
    // Output format
    int output_frames;      // Number of frames in the output tensor (time dimension)
    // Output tensor shape will be [output_frames, num_mfcc]
} eif_audio_config_t;

// Audio Preprocessor Context
typedef struct {
    eif_audio_config_t config;
    eif_mfcc_config_t mfcc_config;
    eif_stft_config_t stft_config;
    
    // Ring Buffer for incoming audio
    float32_t* audio_buffer;
    int buffer_size;
    int write_pos;
    int read_pos;
    int samples_available;
    
    // Output Buffer (Spectrogram/MFCC features)
    // We treat this as a rolling buffer of features
    float32_t* feature_buffer; // [output_frames * num_mfcc]
    int feature_write_pos;     // Current frame index to write
    
    eif_memory_pool_t* pool;
} eif_audio_preprocessor_t;

// Initialize Audio Preprocessor
eif_status_t eif_audio_init(eif_audio_preprocessor_t* ctx, const eif_audio_config_t* config, eif_memory_pool_t* pool);

// Push audio samples into the pipeline
// Returns EIF_STATUS_OK if successful.
// If enough samples are accumulated to process a frame, it processes them and updates the feature buffer.
eif_status_t eif_audio_push(eif_audio_preprocessor_t* ctx, const float32_t* samples, int num_samples);

// Check if enough features are available for inference (i.e., buffer is full or ready)
bool eif_audio_is_ready(const eif_audio_preprocessor_t* ctx);

// Get the current feature tensor (flat buffer)
// Returns pointer to the feature buffer.
const float32_t* eif_audio_get_features(const eif_audio_preprocessor_t* ctx);

// Reset the pipeline (clear buffers)
void eif_audio_reset(eif_audio_preprocessor_t* ctx);

#endif // EIF_AUDIO_H
