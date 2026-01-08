/**
 * @file ascii_plot.h
 * @brief ASCII visualization utilities for EIF tutorials
 * 
 * Provides functions to display waveforms, spectrums, bar charts,
 * heatmaps, and progress bars in the terminal.
 */

#ifndef ASCII_PLOT_H
#define ASCII_PLOT_H

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "eif_types.h"

// ============================================================================
// Configuration
// ============================================================================

#define ASCII_PLOT_WIDTH  60
#define ASCII_PLOT_HEIGHT 15
#define ASCII_MAX_LABEL   20

// Colors (ANSI escape codes)
#define ASCII_RESET   "\033[0m"
#define ASCII_RED     "\033[31m"
#define ASCII_GREEN   "\033[32m"
#define ASCII_YELLOW  "\033[33m"
#define ASCII_BLUE    "\033[34m"
#define ASCII_MAGENTA "\033[35m"
#define ASCII_CYAN    "\033[36m"
#define ASCII_BOLD    "\033[1m"

// ============================================================================
// Waveform Plot
// ============================================================================

/**
 * @brief Plot a time-series waveform
 * 
 * @param title Plot title
 * @param data Data array
 * @param len Number of samples
 * @param width Plot width (characters)
 * @param height Plot height (rows)
 */
static inline void ascii_plot_waveform(const char* title, const float32_t* data, int len, int width, int height) {
    if (!data || len <= 0) return;
    
    // Find min/max
    float32_t min_val = data[0], max_val = data[0];
    for (int i = 1; i < len; i++) {
        if (data[i] < min_val) min_val = data[i];
        if (data[i] > max_val) max_val = data[i];
    }
    
    float32_t range = max_val - min_val;
    if (range < 1e-6f) range = 1.0f;
    
    // Print title
    printf("\n%s╔═", ASCII_CYAN);
    for (int i = 0; i < width; i++) printf("═");
    printf("═╗%s\n", ASCII_RESET);
    printf("%s║%s %s%-*s%s %s║%s\n", ASCII_CYAN, ASCII_RESET, ASCII_BOLD, width, title, ASCII_RESET, ASCII_CYAN, ASCII_RESET);
    printf("%s╠═", ASCII_CYAN);
    for (int i = 0; i < width; i++) printf("═");
    printf("═╣%s\n", ASCII_RESET);
    
    // Print plot
    char line[256];
    for (int row = 0; row < height; row++) {
        float32_t y_level = max_val - (row * range / (height - 1));
        memset(line, ' ', width);
        line[width] = '\0';
        
        // Plot points
        for (int col = 0; col < width; col++) {
            int idx = col * len / width;
            float32_t val_row = (int)((max_val - data[idx]) / range * (height - 1) + 0.5f);
            if ((int)val_row == row) {
                line[col] = '*';
            }
        }
        
        // Zero line
        if (min_val <= 0 && max_val >= 0) {
            int zero_row = (int)((max_val - 0) / range * (height - 1) + 0.5f);
            if (row == zero_row) {
                for (int c = 0; c < width; c++) {
                    if (line[c] == ' ') line[c] = '-';
                }
            }
        }
        
        printf("%s║%s %s%s%s %s║%s %+.2f\n", 
               ASCII_CYAN, ASCII_RESET, ASCII_GREEN, line, ASCII_RESET, ASCII_CYAN, ASCII_RESET, y_level);
    }
    
    printf("%s╚═", ASCII_CYAN);
    for (int i = 0; i < width; i++) printf("═");
    printf("═╝%s\n", ASCII_RESET);
}

// ============================================================================
// Spectrum Plot (Vertical Bars)
// ============================================================================

/**
 * @brief Plot a frequency spectrum with vertical bars
 */
static inline void ascii_plot_spectrum(const char* title, const float32_t* data, int len, int width, int height) {
    if (!data || len <= 0) return;
    
    float32_t max_val = data[0];
    for (int i = 1; i < len; i++) {
        if (data[i] > max_val) max_val = data[i];
    }
    if (max_val < 1e-6f) max_val = 1.0f;
    
    printf("\n%s=== %s ===%s\n", ASCII_BOLD, title, ASCII_RESET);
    
    int bins = (width < len) ? width : len;
    
    for (int row = height; row > 0; row--) {
        printf("  ");
        for (int col = 0; col < bins; col++) {
            int idx = col * len / bins;
            int bar_height = (int)(data[idx] / max_val * height + 0.5f);
            if (bar_height >= row) {
                if (bar_height > height * 0.8) printf("%s█%s", ASCII_RED, ASCII_RESET);
                else if (bar_height > height * 0.5) printf("%s█%s", ASCII_YELLOW, ASCII_RESET);
                else printf("%s█%s", ASCII_GREEN, ASCII_RESET);
            } else {
                printf(" ");
            }
        }
        printf("\n");
    }
    
    // X-axis
    printf("  ");
    for (int i = 0; i < bins; i++) printf("─");
    printf("\n");
}

// ============================================================================
// Horizontal Bar Chart
// ============================================================================

/**
 * @brief Print a horizontal bar chart
 */
static inline void ascii_bar_chart(const char* title, const char** labels, const float32_t* values, int count, int bar_width) {
    printf("\n%s=== %s ===%s\n", ASCII_BOLD, title, ASCII_RESET);
    
    float32_t max_val = values[0];
    for (int i = 1; i < count; i++) {
        if (values[i] > max_val) max_val = values[i];
    }
    if (max_val < 1e-6f) max_val = 1.0f;
    
    for (int i = 0; i < count; i++) {
        int bar_len = (int)(values[i] / max_val * bar_width);
        printf("  %-12s │", labels[i]);
        
        for (int j = 0; j < bar_len; j++) {
            if (values[i] < max_val * 0.3f) printf("%s█%s", ASCII_GREEN, ASCII_RESET);
            else if (values[i] < max_val * 0.7f) printf("%s█%s", ASCII_YELLOW, ASCII_RESET);
            else printf("%s█%s", ASCII_RED, ASCII_RESET);
        }
        printf(" %.2f\n", values[i]);
    }
}

// ============================================================================
// 2D Heatmap / Grid
// ============================================================================

/**
 * @brief Display a 2D grid/heatmap
 */
static inline void ascii_heatmap(const char* title, const float32_t* data, int rows, int cols) {
    const char* shades = " .:-=+*#%@";
    int num_shades = 10;
    
    float32_t min_val = data[0], max_val = data[0];
    for (int i = 0; i < rows * cols; i++) {
        if (data[i] < min_val) min_val = data[i];
        if (data[i] > max_val) max_val = data[i];
    }
    float32_t range = max_val - min_val;
    if (range < 1e-6f) range = 1.0f;
    
    printf("\n%s=== %s ===%s\n", ASCII_BOLD, title, ASCII_RESET);
    
    printf("  ┌");
    for (int c = 0; c < cols; c++) printf("─");
    printf("┐\n");
    
    for (int r = 0; r < rows; r++) {
        printf("  │");
        for (int c = 0; c < cols; c++) {
            int idx = (int)((data[r * cols + c] - min_val) / range * (num_shades - 1));
            if (idx < 0) idx = 0;
            if (idx >= num_shades) idx = num_shades - 1;
            printf("%c", shades[idx]);
        }
        printf("│\n");
    }
    
    printf("  └");
    for (int c = 0; c < cols; c++) printf("─");
    printf("┘\n");
}

// ============================================================================
// Progress Bar
// ============================================================================

/**
 * @brief Print a progress bar
 */
static inline void ascii_progress(const char* label, float32_t progress, int width) {
    int filled = (int)(progress * width);
    printf("\r  %s [", label);
    for (int i = 0; i < width; i++) {
        if (i < filled) printf("%s█%s", ASCII_GREEN, ASCII_RESET);
        else printf(" ");
    }
    printf("] %5.1f%%", progress * 100);
    fflush(stdout);
}

// ============================================================================
// Section Header
// ============================================================================

static inline void ascii_section(const char* title) {
    int len = strlen(title);
    printf("\n%s╔", ASCII_CYAN);
    for (int i = 0; i < len + 4; i++) printf("═");
    printf("╗%s\n", ASCII_RESET);
    printf("%s║  %s%s%s  ║%s\n", ASCII_CYAN, ASCII_BOLD, title, ASCII_RESET ASCII_CYAN, ASCII_RESET);
    printf("%s╚", ASCII_CYAN);
    for (int i = 0; i < len + 4; i++) printf("═");
    printf("╝%s\n\n", ASCII_RESET);
}

// ============================================================================
// Table Printing
// ============================================================================

static inline void ascii_table_header(const char** headers, int count) {
    printf("\n  ");
    for (int i = 0; i < count; i++) {
        printf("%s%-15s%s ", ASCII_BOLD, headers[i], ASCII_RESET);
    }
    printf("\n  ");
    for (int i = 0; i < count; i++) {
        printf("─────────────── ");
    }
    printf("\n");
}

static inline void ascii_table_row(const char** values, int count) {
    printf("  ");
    for (int i = 0; i < count; i++) {
        printf("%-15s ", values[i]);
    }
    printf("\n");
}

// ============================================================================
// Info Box
// ============================================================================

static inline void ascii_info(const char* text) {
    printf("  %s[INFO]%s %s\n", ASCII_BLUE, ASCII_RESET, text);
}

static inline void ascii_success(const char* text) {
    printf("  %s[OK]%s %s\n", ASCII_GREEN, ASCII_RESET, text);
}

static inline void ascii_warning(const char* text) {
    printf("  %s[WARN]%s %s\n", ASCII_YELLOW, ASCII_RESET, text);
}

static inline void ascii_error(const char* text) {
    printf("  %s[ERR]%s %s\n", ASCII_RED, ASCII_RESET, text);
}

#endif // ASCII_PLOT_H
