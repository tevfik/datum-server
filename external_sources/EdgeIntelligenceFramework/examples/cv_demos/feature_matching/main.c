/**
 * @file main.c
 * @brief Feature Detection Demo with PPM Visualization and JSON Output
 * 
 * Features:
 * - FAST corner detection
 * - Harris corner detection
 * - Template matching
 * - JSON output for automation and visualization
 * 
 * Usage:
 *   ./feature_matching_demo              # Standard output
 *   ./feature_matching_demo --json       # JSON summary
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "eif_cv.h"

#define IMG_WIDTH  128
#define IMG_HEIGHT 128

static uint8_t pool_buffer[1024 * 1024];
static eif_memory_pool_t pool;

static bool json_mode = false;

static void generate_checker_image(eif_cv_image_t* img) {
    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            img->data[y * img->stride + x] = ((y / 32 + x / 32) % 2) ? 200 : 50;
        }
    }
}

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  --json        Output JSON summary for automation\n");
    printf("  --help        Show this help\n");
    printf("\nVisualization:\n");
    printf("  python3 tools/cv_visualizer.py --grid output/*.ppm\n");
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
        printf("\n=== EIF CV: Feature Detection Demo ===\n\n");
    }
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Create output directory
    #ifdef _WIN32
    _mkdir("output");
    #else
    mkdir("output", 0755);
    #endif
    
    // Create images
    eif_cv_image_t src, templ;
    eif_cv_image_create(&src, IMG_WIDTH, IMG_HEIGHT, EIF_CV_GRAY8, &pool);
    eif_cv_image_create(&templ, 32, 32, EIF_CV_GRAY8, &pool);
    
    // Generate checker pattern
    generate_checker_image(&src);
    eif_cv_write_pgm(&src, "output/01_source.pgm");
    
    if (!json_mode) {
        printf("Saved: output/01_source.pgm\n");
    }
    
    // --- FAST Corner Detection ---
    eif_cv_keypoint_t fast_corners[200];
    int fast_count = eif_cv_detect_fast(&src, 30, true, fast_corners, 200);
    
    if (!json_mode) {
        printf("\n--- FAST Corner Detection ---\n");
        printf("FAST corners detected: %d\n", fast_count);
    }
    
    eif_cv_write_with_keypoints(&src, fast_corners, fast_count, "output/02_fast_corners.ppm");
    
    // --- Harris Corner Detection ---
    eif_cv_keypoint_t harris_corners[200];
    int harris_count = eif_cv_detect_harris(&src, 3, 3, 0.04f, 500.0f, harris_corners, 200, &pool);
    
    if (!json_mode) {
        printf("\n--- Harris Corner Detection ---\n");
        printf("Harris corners detected: %d\n", harris_count);
    }
    
    eif_cv_write_with_keypoints(&src, harris_corners, harris_count, "output/03_harris_corners.ppm");
    
    // --- Template Matching ---
    // Create template from src (32x32 block at position 32,32)
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            templ.data[y * templ.stride + x] = src.data[(32+y) * src.stride + (32+x)];
        }
    }
    eif_cv_write_pgm(&templ, "output/04_template.pgm");
    
    int result_w = IMG_WIDTH - 32 + 1;
    int result_h = IMG_HEIGHT - 32 + 1;
    float32_t* result = eif_memory_alloc(&pool, result_w * result_h * sizeof(float32_t), 4);
    
    eif_cv_match_template(&src, &templ, result, EIF_CV_TM_SQDIFF, &pool);
    eif_cv_tm_result_t match = eif_cv_template_minmax(result, result_w, result_h, EIF_CV_TM_SQDIFF);
    
    if (!json_mode) {
        printf("\n--- Template Matching ---\n");
        printf("Best match at: (%d, %d), score: %.2f\n", match.x, match.y, match.score);
    }
    
    // Draw match location on image
    eif_cv_keypoint_t match_kpt = {match.x + 16, match.y + 16, 32, 0, 1.0f, 0, 0};
    eif_cv_write_with_keypoints(&src, &match_kpt, 1, "output/05_template_match.ppm");
    
    // Output
    if (json_mode) {
        printf("{\"type\": \"cv_pipeline\", \"name\": \"feature_matching\"");
        printf(", \"input\": {\"width\": %d, \"height\": %d}", IMG_WIDTH, IMG_HEIGHT);
        printf(", \"detections\": {");
        printf("\"fast_corners\": %d, \"harris_corners\": %d", fast_count, harris_count);
        printf(", \"template_match\": {\"x\": %d, \"y\": %d, \"score\": %.2f}", match.x, match.y, match.score);
        printf("}");
        printf(", \"outputs\": [");
        printf("{\"name\": \"source\", \"file\": \"output/01_source.pgm\"}");
        printf(", {\"name\": \"fast_corners\", \"file\": \"output/02_fast_corners.ppm\", \"count\": %d}", fast_count);
        printf(", {\"name\": \"harris_corners\", \"file\": \"output/03_harris_corners.ppm\", \"count\": %d}", harris_count);
        printf(", {\"name\": \"template\", \"file\": \"output/04_template.pgm\", \"size\": 32}");
        printf(", {\"name\": \"template_match\", \"file\": \"output/05_template_match.ppm\"}");
        printf("]}\n");
    } else {
        printf("\n--- Summary ---\n");
        printf("  FAST corners:   %d\n", fast_count);
        printf("  Harris corners: %d\n", harris_count);
        printf("  Template match: (%d, %d) score=%.2f\n\n", match.x, match.y, match.score);
        printf("Generated 5 images in output/ directory\n");
        printf("View with: python3 tools/cv_visualizer.py --grid output/*.ppm\n\n");
    }
    
    return 0;
}
