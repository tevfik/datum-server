/**
 * @file main.c
 * @brief Bearing Fault Detection Demo - Interactive Predictive Maintenance
 * 
 * Demonstrates vibration-based bearing fault detection using:
 * - FFT spectral analysis
 * - Bearing defect frequencies (BPFO, BPFI, BSF, FTF)
 * - Interactive fault injection via keyboard
 * - JSON output for visualization
 * 
 * Commands:
 *   n - Set to Normal (healthy)
 *   o - Inject Outer Race fault
 *   i - Inject Inner Race fault
 *   b - Inject Ball fault
 *   j - Toggle JSON output
 *   q - Quit
 * 
 * Usage:
 *   ./bearing_fault_demo              # Interactive mode
 *   ./bearing_fault_demo --json       # JSON output for plotter
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <poll.h>

#include "eif_memory.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Bearing Parameters (Example: SKF 6205)
// ============================================================================

typedef struct {
    const char* name;
    int n_balls;
    float ball_dia;
    float pitch_dia;
    float contact_angle;
} bearing_params_t;

static const bearing_params_t SKF_6205 = {
    .name = "SKF 6205",
    .n_balls = 9,
    .ball_dia = 7.94f,
    .pitch_dia = 38.5f,
    .contact_angle = 0.0f
};

// ============================================================================
// Bearing Defect Frequencies
// ============================================================================

typedef struct {
    float bpfo, bpfi, bsf, ftf;
} defect_freqs_t;

static void calculate_defect_frequencies(const bearing_params_t* b, float rpm, defect_freqs_t* f) {
    float shaft_freq = rpm / 60.0f;
    float d = b->ball_dia;
    float D = b->pitch_dia;
    float n = (float)b->n_balls;
    float theta = b->contact_angle * M_PI / 180.0f;
    
    float ratio = d / D * cosf(theta);
    
    f->bpfo = 0.5f * n * shaft_freq * (1.0f - ratio);
    f->bpfi = 0.5f * n * shaft_freq * (1.0f + ratio);
    f->bsf = 0.5f * D / d * shaft_freq * (1.0f - ratio * ratio);
    f->ftf = 0.5f * shaft_freq * (1.0f - ratio);
}

// ============================================================================
// Configuration
// ============================================================================

#define SAMPLE_RATE 10000
#define FFT_SIZE 1024

static uint8_t pool_buffer[128 * 1024];
static eif_memory_pool_t pool;

typedef enum {
    FAULT_NONE,
    FAULT_OUTER_RACE,
    FAULT_INNER_RACE,
    FAULT_BALL,
    FAULT_CAGE
} fault_type_t;

static const char* fault_names[] = {
    "HEALTHY", "OUTER_RACE", "INNER_RACE", "BALL_DEFECT", "CAGE_FAULT"
};

// Runtime state
static fault_type_t current_fault = FAULT_NONE;
static bool json_output = false;
static bool running = true;
static float rpm = 1800.0f;

// ============================================================================
// Keyboard Input (Non-blocking)
// ============================================================================

static struct termios orig_termios;
static bool termios_saved = false;

static void disable_raw_mode(void) {
    if (termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    }
}

static void enable_raw_mode(void) {
    if (!termios_saved) {
        tcgetattr(STDIN_FILENO, &orig_termios);
        termios_saved = true;
        atexit(disable_raw_mode);
    }
    
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static int get_keypress(void) {
    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
    if (poll(&pfd, 1, 0) > 0) {
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            return c;
        }
    }
    return -1;
}

// ============================================================================
// Signal Generation and Analysis
// ============================================================================

static void generate_vibration(float* signal, int len, float rpm, fault_type_t fault) {
    defect_freqs_t freqs;
    calculate_defect_frequencies(&SKF_6205, rpm, &freqs);
    
    float shaft_freq = rpm / 60.0f;
    
    for (int i = 0; i < len; i++) {
        float t = (float)i / SAMPLE_RATE;
        
        signal[i] = 0.5f * sinf(2 * M_PI * shaft_freq * t);
        signal[i] += 0.1f * ((float)rand() / RAND_MAX - 0.5f);
        
        switch (fault) {
            case FAULT_OUTER_RACE:
                signal[i] += 2.0f * sinf(2 * M_PI * freqs.bpfo * t) * 
                             (1 + 0.5f * sinf(2 * M_PI * shaft_freq * t));
                signal[i] += 0.8f * sinf(2 * M_PI * 2 * freqs.bpfo * t);
                break;
                
            case FAULT_INNER_RACE:
                signal[i] += 1.5f * sinf(2 * M_PI * freqs.bpfi * t) *
                             (1 + 0.7f * sinf(2 * M_PI * shaft_freq * t));
                break;
                
            case FAULT_BALL:
                signal[i] += 1.0f * sinf(2 * M_PI * freqs.bsf * t);
                signal[i] += 0.7f * sinf(2 * M_PI * 2 * freqs.bsf * t);
                break;
                
            case FAULT_CAGE:
                signal[i] += 0.8f * sinf(2 * M_PI * (freqs.bpfo + freqs.ftf) * t);
                signal[i] += 0.8f * sinf(2 * M_PI * (freqs.bpfo - freqs.ftf) * t);
                break;
                
            default:
                break;
        }
    }
}

static void compute_spectrum(const float* signal, float* spectrum, int n) {
    for (int k = 0; k < n/2; k++) {
        float real = 0, imag = 0;
        for (int i = 0; i < n; i++) {
            float angle = 2 * M_PI * k * i / n;
            real += signal[i] * cosf(angle);
            imag -= signal[i] * sinf(angle);
        }
        spectrum[k] = sqrtf(real*real + imag*imag) / n;
    }
}

static float find_peak_amplitude(const float* spectrum, int n, float target_freq, float tolerance) {
    int bin_low = (int)((target_freq - tolerance) * n / SAMPLE_RATE);
    int bin_high = (int)((target_freq + tolerance) * n / SAMPLE_RATE);
    
    if (bin_low < 0) bin_low = 0;
    if (bin_high > n/2) bin_high = n/2;
    
    float max_amp = 0;
    for (int k = bin_low; k <= bin_high; k++) {
        if (spectrum[k] > max_amp) max_amp = spectrum[k];
    }
    return max_amp;
}

static fault_type_t diagnose_bearing(const float* spectrum, int n, float rpm) {
    defect_freqs_t freqs;
    calculate_defect_frequencies(&SKF_6205, rpm, &freqs);
    
    float tolerance = 5.0f;
    float threshold = 0.3f;
    
    float bpfo_amp = find_peak_amplitude(spectrum, n, freqs.bpfo, tolerance);
    float bpfi_amp = find_peak_amplitude(spectrum, n, freqs.bpfi, tolerance);
    float bsf_amp = find_peak_amplitude(spectrum, n, freqs.bsf, tolerance);
    float ftf_amp = find_peak_amplitude(spectrum, n, freqs.ftf, tolerance);
    
    if (bpfo_amp > threshold && bpfo_amp > bpfi_amp && bpfo_amp > bsf_amp) {
        return FAULT_OUTER_RACE;
    }
    if (bpfi_amp > threshold && bpfi_amp > bsf_amp) {
        return FAULT_INNER_RACE;
    }
    if (bsf_amp > threshold) {
        return FAULT_BALL;
    }
    if (ftf_amp > threshold * 0.5f) {
        return FAULT_CAGE;
    }
    
    return FAULT_NONE;
}

// ============================================================================
// Output Functions
// ============================================================================

static void print_json(int timestamp, float bpfo, float bpfi, float bsf, fault_type_t diagnosed) {
    printf("{\"timestamp\": %d, ", timestamp);
    printf("\"signals\": [%.4f, %.4f, %.4f], ", bpfo, bpfi, bsf);
    printf("\"actual_fault\": \"%s\", ", fault_names[current_fault]);
    printf("\"prediction\": \"%s\"}\n", fault_names[diagnosed]);
    fflush(stdout);
}

static void print_status(float bpfo, float bpfi, float bsf, fault_type_t diagnosed) {
    printf("\r  Fault: %-12s | BPFO: %5.2f | BPFI: %5.2f | BSF: %5.2f | Diagnosis: %-12s  ",
           fault_names[current_fault], bpfo, bpfi, bsf, fault_names[diagnosed]);
    fflush(stdout);
}

static void print_help(void) {
    printf("\n");
    printf("+----------------------------------------------------------+\n");
    printf("|  Interactive Bearing Fault Detection                     |\n");
    printf("+----------------------------------------------------------+\n");
    printf("|  Keys:                                                   |\n");
    printf("|    n - Normal (healthy)                                  |\n");
    printf("|    o - Outer race fault                                  |\n");
    printf("|    i - Inner race fault                                  |\n");
    printf("|    b - Ball defect                                       |\n");
    printf("|    j - Toggle JSON output                                |\n");
    printf("|    h - Show this help                                    |\n");
    printf("|    q - Quit                                              |\n");
    printf("+----------------------------------------------------------+\n\n");
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    srand(time(NULL));
    
    // Parse args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_output = true;
        }
    }
    
    if (!json_output) {
        printf("\n");
        printf("+----------------------------------------------------------+\n");
        printf("|    Bearing Fault Detection Demo - Interactive Mode       |\n");
        printf("+----------------------------------------------------------+\n\n");
        
        defect_freqs_t freqs;
        calculate_defect_frequencies(&SKF_6205, rpm, &freqs);
        
        printf("  Bearing: %s @ %.0f RPM\n", SKF_6205.name, rpm);
        printf("  BPFO: %.1f Hz | BPFI: %.1f Hz | BSF: %.1f Hz\n\n", 
               freqs.bpfo, freqs.bpfi, freqs.bsf);
        
        print_help();
        printf("Starting real-time analysis (press keys to inject faults)...\n\n");
        
        enable_raw_mode();
    }
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    float* signal = eif_memory_alloc(&pool, FFT_SIZE * sizeof(float), 4);
    float* spectrum = eif_memory_alloc(&pool, FFT_SIZE * sizeof(float), 4);
    
    int timestamp = 0;
    
    while (running) {
        // Check for keypress
        int key = get_keypress();
        if (key >= 0) {
            switch (key) {
                case 'n': case 'N':
                    current_fault = FAULT_NONE;
                    if (!json_output) printf("\n[SET: HEALTHY]\n");
                    break;
                case 'o': case 'O':
                    current_fault = FAULT_OUTER_RACE;
                    if (!json_output) printf("\n[SET: OUTER RACE FAULT]\n");
                    break;
                case 'i': case 'I':
                    current_fault = FAULT_INNER_RACE;
                    if (!json_output) printf("\n[SET: INNER RACE FAULT]\n");
                    break;
                case 'b': case 'B':
                    current_fault = FAULT_BALL;
                    if (!json_output) printf("\n[SET: BALL DEFECT]\n");
                    break;
                case 'j': case 'J':
                    json_output = !json_output;
                    if (!json_output) printf("\n[JSON: OFF]\n");
                    break;
                case 'h': case 'H':
                    if (!json_output) print_help();
                    break;
                case 'q': case 'Q':
                case 3:  // Ctrl+C
                    running = false;
                    break;
            }
        }
        
        // Generate and analyze
        generate_vibration(signal, FFT_SIZE, rpm, current_fault);
        compute_spectrum(signal, spectrum, FFT_SIZE);
        
        defect_freqs_t freqs;
        calculate_defect_frequencies(&SKF_6205, rpm, &freqs);
        
        float bpfo_amp = find_peak_amplitude(spectrum, FFT_SIZE, freqs.bpfo, 5.0f);
        float bpfi_amp = find_peak_amplitude(spectrum, FFT_SIZE, freqs.bpfi, 5.0f);
        float bsf_amp = find_peak_amplitude(spectrum, FFT_SIZE, freqs.bsf, 5.0f);
        
        fault_type_t diagnosed = diagnose_bearing(spectrum, FFT_SIZE, rpm);
        
        // Output
        if (json_output) {
            print_json(timestamp, bpfo_amp, bpfi_amp, bsf_amp, diagnosed);
        } else {
            print_status(bpfo_amp, bpfi_amp, bsf_amp, diagnosed);
        }
        
        timestamp++;
        usleep(100000);  // 100ms = 10Hz update rate
    }
    
    if (!json_output) {
        printf("\n\nDemo complete!\n\n");
    }
    
    return 0;
}
