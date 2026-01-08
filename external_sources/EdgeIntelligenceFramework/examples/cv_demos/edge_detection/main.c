/**
 * @file main.c
 * @brief Edge Detection Demo with PPM Visualization and JSON Output
 * 
 * Features:
 * - Gaussian blur preprocessing
 * - Sobel gradient computation
 * - Canny edge detection
 * - JSON annotation output for cv_visualizer.py
 * 
 * Usage:
 *   ./edge_detection_demo                    # Standard output + images
 *   ./edge_detection_demo --json             # JSON summary for automation
 *   python3 tools/cv_visualizer.py --image output/05_gradient_magnitude.pgm --histogram
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "eif_cv.h"

#define IMG_WIDTH  128
#define IMG_HEIGHT 128

static uint8_t pool_buffer[512 * 1024];
static eif_memory_pool_t pool;

static bool json_mode = false;

// Generate test image with geometric shapes
static void generate_test_image(eif_cv_image_t* img) {
    memset(img->data, 50, img->stride * img->height);
    
    // Draw rectangle
    for (int y = 20; y < 60; y++) {
        for (int x = 20; x < 80; x++) {
            if (y == 20 || y == 59 || x == 20 || x == 79) {
                img->data[y * img->stride + x] = 200;
            }
        }
    }
    
    // Draw filled circle
    int cx = 96, cy = 96, r = 20;
    for (int y = cy - r; y <= cy + r; y++) {
        for (int x = cx - r; x <= cx + r; x++) {
            int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy <= r * r) {
                if (x >= 0 && x < IMG_WIDTH && y >= 0 && y < IMG_HEIGHT) {
                    img->data[y * img->stride + x] = 180;
                }
            }
        }
    }
    
    // Gradient stripe
    for (int y = 70; y < 90; y++) {
        for (int x = 20; x < 80; x++) {
            img->data[y * img->stride + x] = 50 + (x - 20) * 3;
        }
    }
}

// Count edge pixels
static int count_edge_pixels(const eif_cv_image_t* img) {
    int count = 0;
    for (int i = 0; i < img->width * img->height; i++) {
        if (img->data[i] > 0) count++;
    }
    return count;
}

// Compute image statistics
static void compute_stats(const eif_cv_image_t* img, float* mean, float* max_val) {
    double sum = 0;
    *max_val = 0;
    int size = img->width * img->height;
    
    for (int i = 0; i < size; i++) {
        sum += img->data[i];
        if (img->data[i] > *max_val) *max_val = img->data[i];
    }
    *mean = (float)(sum / size);
}

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  --json        Output JSON summary for automation\n");
    printf("  --help        Show this help\n");
    printf("\nVisualization:\n");
    printf("  python3 tools/cv_visualizer.py --image output/05_gradient_magnitude.pgm --histogram\n");
    printf("  python3 tools/cv_visualizer.py --compare output/01_source.pgm output/06_canny_edges.pgm\n");
    printf("  python3 tools/cv_visualizer.py --grid output/*.pgm\n");
}

int main(int argc, char** argv) {
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    if (!json_mode) {
        printf("\n=== EIF CV: Edge Detection Demo ===\n\n");
    }
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Create output directory
    #ifdef _WIN32
    _mkdir("output");
    #else
    mkdir("output", 0755);
    #endif
    
    // Create images
    eif_cv_image_t src, sobel_x, sobel_y, magnitude, canny, blurred;
    eif_cv_image_create(&src, IMG_WIDTH, IMG_HEIGHT, EIF_CV_GRAY8, &pool);
    eif_cv_image_create(&sobel_x, IMG_WIDTH, IMG_HEIGHT, EIF_CV_GRAY8, &pool);
    eif_cv_image_create(&sobel_y, IMG_WIDTH, IMG_HEIGHT, EIF_CV_GRAY8, &pool);
    eif_cv_image_create(&magnitude, IMG_WIDTH, IMG_HEIGHT, EIF_CV_GRAY8, &pool);
    eif_cv_image_create(&canny, IMG_WIDTH, IMG_HEIGHT, EIF_CV_GRAY8, &pool);
    eif_cv_image_create(&blurred, IMG_WIDTH, IMG_HEIGHT, EIF_CV_GRAY8, &pool);
    
    // Generate test image
    generate_test_image(&src);
    eif_cv_write_pgm(&src, "output/01_source.pgm");
    
    // Gaussian blur
    eif_cv_blur_gaussian(&src, &blurred, 5, 1.0f, &pool);
    eif_cv_write_pgm(&blurred, "output/02_blurred.pgm");
    
    // Sobel gradients
    eif_cv_sobel(&src, &sobel_x, 1, 0, 3);
    eif_cv_sobel(&src, &sobel_y, 0, 1, 3);
    eif_cv_write_pgm(&sobel_x, "output/03_sobel_x.pgm");
    eif_cv_write_pgm(&sobel_y, "output/04_sobel_y.pgm");
    
    // Gradient magnitude
    eif_cv_gradient_magnitude(&sobel_x, &sobel_y, &magnitude);
    eif_cv_write_pgm(&magnitude, "output/05_gradient_magnitude.pgm");
    
    // Canny edge detection
    eif_cv_canny(&src, &canny, 30, 100, &pool);
    eif_cv_write_pgm(&canny, "output/06_canny_edges.pgm");
    
    // Compute statistics
    float src_mean, src_max, mag_mean, mag_max, canny_mean, canny_max;
    compute_stats(&src, &src_mean, &src_max);
    compute_stats(&magnitude, &mag_mean, &mag_max);
    compute_stats(&canny, &canny_mean, &canny_max);
    
    int canny_edges = count_edge_pixels(&canny);
    float edge_density = 100.0f * canny_edges / (IMG_WIDTH * IMG_HEIGHT);
    
    if (json_mode) {
        printf("{\"type\": \"cv_pipeline\", \"name\": \"edge_detection\"");
        printf(", \"input\": {\"width\": %d, \"height\": %d, \"mean\": %.1f}", IMG_WIDTH, IMG_HEIGHT, src_mean);
        printf(", \"outputs\": [");
        printf("{\"name\": \"source\", \"file\": \"output/01_source.pgm\"}");
        printf(", {\"name\": \"blurred\", \"file\": \"output/02_blurred.pgm\", \"kernel\": 5}");
        printf(", {\"name\": \"sobel_x\", \"file\": \"output/03_sobel_x.pgm\"}");
        printf(", {\"name\": \"sobel_y\", \"file\": \"output/04_sobel_y.pgm\"}");
        printf(", {\"name\": \"magnitude\", \"file\": \"output/05_gradient_magnitude.pgm\", \"mean\": %.1f, \"max\": %.0f}", mag_mean, mag_max);
        printf(", {\"name\": \"canny\", \"file\": \"output/06_canny_edges.pgm\", \"edge_pixels\": %d, \"density\": %.1f}", canny_edges, edge_density);
        printf("]");
        printf(", \"summary\": {\"total_files\": 6, \"edge_density\": %.1f}}\n", edge_density);
    } else {
        printf("Generated images:\n");
        printf("  1. output/01_source.pgm           (source image)\n");
        printf("  2. output/02_blurred.pgm          (Gaussian blur, k=5)\n");
        printf("  3. output/03_sobel_x.pgm          (Sobel X gradient)\n");
        printf("  4. output/04_sobel_y.pgm          (Sobel Y gradient)\n");
        printf("  5. output/05_gradient_magnitude.pgm (magnitude, mean=%.1f)\n", mag_mean);
        printf("  6. output/06_canny_edges.pgm      (Canny, %d edges, %.1f%% density)\n\n", canny_edges, edge_density);
        
        printf("Visualization commands:\n");
        printf("  python3 tools/cv_visualizer.py --image output/05_gradient_magnitude.pgm --histogram\n");
        printf("  python3 tools/cv_visualizer.py --compare output/01_source.pgm output/06_canny_edges.pgm\n");
        printf("  python3 tools/cv_visualizer.py --grid output/*.pgm\n\n");
    }
    
    return 0;
}
