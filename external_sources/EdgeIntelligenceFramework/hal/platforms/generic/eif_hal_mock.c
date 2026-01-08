/**
 * @file eif_hal_mock.c
 * @brief Mock Hardware Implementation for PC Testing
 * 
 * Implements synthetic sensors for desktop development.
 */

#include "eif_hal_mock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Mock IMU State
// ============================================================================

static struct {
    eif_mock_imu_mode_t mode;
    float sample_rate;
    float noise_level;
    float frequency;
    float amplitude;
    uint64_t sample_count;
    FILE* csv_file;
    
    // Fault injection
    float fault_magnitude;
    int fault_remaining;
} s_mock_imu = {
    .mode = EIF_MOCK_IMU_SINE,
    .sample_rate = 100.0f,
    .noise_level = 0.1f,
    .frequency = 1.0f,
    .amplitude = 1.0f
};

// Gaussian random
static float gaussian_rand(void) {
    static int has_spare = 0;
    static float spare;
    
    if (has_spare) {
        has_spare = 0;
        return spare;
    }
    
    float u, v, s;
    do {
        u = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        v = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        s = u * u + v * v;
    } while (s >= 1.0f || s == 0.0f);
    
    s = sqrtf(-2.0f * logf(s) / s);
    spare = v * s;
    has_spare = 1;
    return u * s;
}

int eif_mock_imu_init(const eif_mock_imu_config_t* config) {
    if (!config) return -1;
    
    s_mock_imu.mode = config->mode;
    s_mock_imu.sample_rate = config->sample_rate > 0 ? config->sample_rate : 100.0f;
    s_mock_imu.noise_level = config->noise_level;
    s_mock_imu.frequency = config->frequency > 0 ? config->frequency : 1.0f;
    s_mock_imu.amplitude = config->amplitude > 0 ? config->amplitude : 1.0f;
    s_mock_imu.sample_count = 0;
    s_mock_imu.fault_magnitude = 0;
    s_mock_imu.fault_remaining = 0;
    
    if (config->mode == EIF_MOCK_IMU_CSV && config->csv_path) {
        s_mock_imu.csv_file = fopen(config->csv_path, "r");
        if (!s_mock_imu.csv_file) {
            fprintf(stderr, "Mock IMU: Cannot open CSV file: %s\n", config->csv_path);
            return -1;
        }
        // Skip header line
        char header[256];
        if (fgets(header, sizeof(header), s_mock_imu.csv_file)) {}
    }
    
    srand(time(NULL));
    return 0;
}

int eif_mock_imu_read_accel(float* ax, float* ay, float* az) {
    if (!ax || !ay || !az) return -1;
    
    float t = (float)s_mock_imu.sample_count / s_mock_imu.sample_rate;
    float fault = s_mock_imu.fault_remaining > 0 ? s_mock_imu.fault_magnitude : 0;
    
    switch (s_mock_imu.mode) {
        case EIF_MOCK_IMU_SINE:
            *ax = s_mock_imu.amplitude * sinf(2 * M_PI * s_mock_imu.frequency * t);
            *ay = s_mock_imu.amplitude * sinf(2 * M_PI * s_mock_imu.frequency * t + M_PI/3);
            *az = 9.81f + s_mock_imu.amplitude * sinf(2 * M_PI * s_mock_imu.frequency * t + 2*M_PI/3);
            break;
            
        case EIF_MOCK_IMU_STATIONARY:
            *ax = 0;
            *ay = 0;
            *az = 9.81f;
            break;
            
        case EIF_MOCK_IMU_NOISE:
            *ax = gaussian_rand() * s_mock_imu.amplitude;
            *ay = gaussian_rand() * s_mock_imu.amplitude;
            *az = 9.81f + gaussian_rand() * s_mock_imu.amplitude;
            break;
            
        case EIF_MOCK_IMU_CSV:
            if (s_mock_imu.csv_file) {
                if (fscanf(s_mock_imu.csv_file, "%f,%f,%f", ax, ay, az) != 3) {
                    // Restart from beginning
                    fseek(s_mock_imu.csv_file, 0, SEEK_SET);
                    char header[256];
                    if (fgets(header, sizeof(header), s_mock_imu.csv_file)) {}
                    fscanf(s_mock_imu.csv_file, "%f,%f,%f", ax, ay, az);
                }
            }
            break;
    }
    
    // Add noise
    if (s_mock_imu.noise_level > 0) {
        *ax += gaussian_rand() * s_mock_imu.noise_level;
        *ay += gaussian_rand() * s_mock_imu.noise_level;
        *az += gaussian_rand() * s_mock_imu.noise_level;
    }
    
    // Apply fault
    *ax += fault;
    *ay += fault;
    
    if (s_mock_imu.fault_remaining > 0) {
        s_mock_imu.fault_remaining--;
    }
    
    s_mock_imu.sample_count++;
    return 0;
}

int eif_mock_imu_read_gyro(float* gx, float* gy, float* gz) {
    if (!gx || !gy || !gz) return -1;
    
    float t = (float)s_mock_imu.sample_count / s_mock_imu.sample_rate;
    
    switch (s_mock_imu.mode) {
        case EIF_MOCK_IMU_SINE:
            *gx = 0.1f * sinf(2 * M_PI * 0.5f * t);
            *gy = 0.1f * cosf(2 * M_PI * 0.5f * t);
            *gz = 0.05f * sinf(2 * M_PI * 0.2f * t);
            break;
            
        case EIF_MOCK_IMU_STATIONARY:
        case EIF_MOCK_IMU_NOISE:
            *gx = gaussian_rand() * 0.01f;
            *gy = gaussian_rand() * 0.01f;
            *gz = gaussian_rand() * 0.01f;
            break;
            
        default:
            *gx = *gy = *gz = 0;
            break;
    }
    
    // Add noise
    if (s_mock_imu.noise_level > 0) {
        *gx += gaussian_rand() * s_mock_imu.noise_level * 0.01f;
        *gy += gaussian_rand() * s_mock_imu.noise_level * 0.01f;
        *gz += gaussian_rand() * s_mock_imu.noise_level * 0.01f;
    }
    
    return 0;
}

void eif_mock_imu_set_mode(eif_mock_imu_mode_t mode) {
    s_mock_imu.mode = mode;
}

void eif_mock_imu_inject_fault(float magnitude, int duration_samples) {
    s_mock_imu.fault_magnitude = magnitude;
    s_mock_imu.fault_remaining = duration_samples;
}

// ============================================================================
// Mock Audio
// ============================================================================

static struct {
    eif_mock_audio_mode_t mode;
    int sample_rate;
    float frequency;
    float amplitude;
    uint64_t sample_count;
    FILE* wav_file;
    int wav_channels;
    int wav_bits;
    long wav_data_start;
    long wav_data_size;
    bool eof;
} s_mock_audio = {
    .mode = EIF_MOCK_AUDIO_TONE,
    .sample_rate = 16000,
    .frequency = 440.0f,
    .amplitude = 0.5f
};

int eif_mock_audio_init(const eif_mock_audio_config_t* config) {
    if (!config) return -1;
    
    s_mock_audio.mode = config->mode;
    s_mock_audio.sample_rate = config->sample_rate > 0 ? config->sample_rate : 16000;
    s_mock_audio.frequency = config->frequency > 0 ? config->frequency : 440.0f;
    s_mock_audio.amplitude = config->amplitude > 0 ? config->amplitude : 0.5f;
    s_mock_audio.sample_count = 0;
    s_mock_audio.eof = false;
    
    if (config->mode == EIF_MOCK_AUDIO_WAV && config->wav_path) {
        s_mock_audio.wav_file = fopen(config->wav_path, "rb");
        if (!s_mock_audio.wav_file) {
            fprintf(stderr, "Mock Audio: Cannot open WAV file: %s\n", config->wav_path);
            return -1;
        }
        
        // Simple WAV header parsing
        char header[44];
        if (fread(header, 1, 44, s_mock_audio.wav_file) < 44) {
            fclose(s_mock_audio.wav_file);
            return -1;
        }
        
        s_mock_audio.wav_channels = *(int16_t*)&header[22];
        s_mock_audio.sample_rate = *(int32_t*)&header[24];
        s_mock_audio.wav_bits = *(int16_t*)&header[34];
        s_mock_audio.wav_data_start = 44;
        s_mock_audio.wav_data_size = *(int32_t*)&header[40];
    }
    
    return 0;
}

int eif_mock_audio_read(float* buffer, int num_samples) {
    if (!buffer || num_samples <= 0) return 0;
    
    int read_count = 0;
    
    switch (s_mock_audio.mode) {
        case EIF_MOCK_AUDIO_TONE:
            for (int i = 0; i < num_samples; i++) {
                float t = (float)(s_mock_audio.sample_count + i) / s_mock_audio.sample_rate;
                buffer[i] = s_mock_audio.amplitude * sinf(2 * M_PI * s_mock_audio.frequency * t);
            }
            s_mock_audio.sample_count += num_samples;
            read_count = num_samples;
            break;
            
        case EIF_MOCK_AUDIO_WHITE_NOISE:
            for (int i = 0; i < num_samples; i++) {
                buffer[i] = s_mock_audio.amplitude * ((float)rand() / RAND_MAX * 2 - 1);
            }
            read_count = num_samples;
            break;
            
        case EIF_MOCK_AUDIO_WAV:
            if (s_mock_audio.wav_file && !s_mock_audio.eof) {
                for (int i = 0; i < num_samples; i++) {
                    int16_t sample;
                    if (fread(&sample, sizeof(int16_t), 1, s_mock_audio.wav_file) == 1) {
                        buffer[i] = (float)sample / 32768.0f;
                        read_count++;
                    } else {
                        s_mock_audio.eof = true;
                        break;
                    }
                }
            }
            break;
            
        default:
            break;
    }
    
    return read_count;
}

bool eif_mock_audio_eof(void) {
    return s_mock_audio.eof;
}

void eif_mock_audio_reset(void) {
    if (s_mock_audio.wav_file) {
        fseek(s_mock_audio.wav_file, s_mock_audio.wav_data_start, SEEK_SET);
        s_mock_audio.eof = false;
    }
    s_mock_audio.sample_count = 0;
}

// ============================================================================
// Mock Timer
// ============================================================================

#include <sys/time.h>

static struct timeval s_timer_start;
static bool s_timer_initialized = false;

static void timer_init_once(void) {
    if (!s_timer_initialized) {
        gettimeofday(&s_timer_start, NULL);
        s_timer_initialized = true;
    }
}

uint64_t eif_mock_timer_get_us(void) {
    timer_init_once();
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec - s_timer_start.tv_sec) * 1000000ULL + 
           (now.tv_usec - s_timer_start.tv_usec);
}

void eif_mock_timer_sleep_us(uint32_t us) {
    struct timespec ts = {
        .tv_sec = us / 1000000,
        .tv_nsec = (us % 1000000) * 1000
    };
    nanosleep(&ts, NULL);
}

uint32_t eif_mock_timer_get_ms(void) {
    return (uint32_t)(eif_mock_timer_get_us() / 1000);
}

// ============================================================================
// Mock Serial (for CLI interaction)
// ============================================================================

#include <unistd.h>
#include <poll.h>
#include <fcntl.h>

bool eif_mock_serial_available(void) {
    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
    return poll(&pfd, 1, 0) > 0;
}

int eif_mock_serial_readline(char* buffer, int max_len) {
    if (!buffer || max_len <= 0) return -1;
    
    // Set non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    char* result = fgets(buffer, max_len, stdin);
    
    // Restore blocking
    fcntl(STDIN_FILENO, F_SETFL, flags);
    
    if (result) {
        // Remove newline
        char* nl = strchr(buffer, '\n');
        if (nl) *nl = '\0';
        return strlen(buffer);
    }
    return 0;
}

void eif_mock_serial_write(const char* str) {
    if (str) {
        printf("%s", str);
        fflush(stdout);
    }
}
