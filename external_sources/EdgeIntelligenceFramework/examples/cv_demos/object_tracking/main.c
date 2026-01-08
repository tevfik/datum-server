/**
 * @file main.c
 * @brief Object Tracking Demo with PPM Visualization and JSON Output
 * 
 * Features:
 * - Background subtraction (Gaussian mixture model)
 * - Multi-object tracker with Kalman prediction
 * - JSON output for tracking data visualization
 * 
 * Usage:
 *   ./object_tracking_demo                   # Standard output
 *   ./object_tracking_demo --json            # JSON tracking data
 *   ./object_tracking_demo --json --frames 50
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <sys/stat.h>
#include "eif_cv.h"

#define IMG_WIDTH  128
#define IMG_HEIGHT 128
#define NUM_FRAMES 20

static uint8_t pool_buffer[1024 * 1024];
static eif_memory_pool_t pool;

static bool json_mode = false;
static int sample_count = 0;

// Simulate moving object
static void generate_frame(eif_cv_image_t* frame, int t) {
    memset(frame->data, 100, frame->stride * frame->height);
    
    // Moving object (square moving diagonally)
    int x = 10 + t * 5;
    int y = 10 + t * 4;
    
    for (int dy = 0; dy < 20; dy++) {
        for (int dx = 0; dx < 20; dx++) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < IMG_WIDTH && py >= 0 && py < IMG_HEIGHT) {
                frame->data[py * frame->stride + px] = 200;
            }
        }
    }
}

// Get object center
static void get_object_center(int t, float* cx, float* cy) {
    *cx = 10 + t * 5 + 10;  // center of 20x20 square
    *cy = 10 + t * 4 + 10;
}

// Count foreground pixels
static int count_fg_pixels(const eif_cv_image_t* mask) {
    int count = 0;
    for (int i = 0; i < mask->width * mask->height; i++) {
        if (mask->data[i] > 0) count++;
    }
    return count;
}

static void output_json_track(int frame, int track_id, float x, float y, 
                               float pred_x, float pred_y, int fg_pixels) {
    printf("{\"timestamp\": %d, \"type\": \"tracking\"", sample_count++);
    printf(", \"frame\": %d", frame);
    printf(", \"tracks\": [{\"id\": %d, \"x\": %.1f, \"y\": %.1f", track_id, x, y);
    printf(", \"pred_x\": %.1f, \"pred_y\": %.1f}]", pred_x, pred_y);
    printf(", \"signals\": {\"fg_pixels\": %d}", fg_pixels);
    printf("}\n");
    fflush(stdout);
}

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  --json        Output JSON tracking data\n");
    printf("  --frames N    Number of frames to process (default: 20)\n");
    printf("  --help        Show this help\n");
    printf("\nExamples:\n");
    printf("  %s --json | python3 tools/eif_plotter.py --stdin\n", prog);
    printf("  %s --json --frames 50\n", prog);
}

int main(int argc, char** argv) {
    int num_frames = NUM_FRAMES;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            num_frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    if (!json_mode) {
        printf("\n=== EIF CV: Object Tracking Demo ===\n\n");
    }
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Create output directory
    #ifdef _WIN32
    _mkdir("output");
    #else
    mkdir("output", 0755);
    #endif
    
    // Create images
    eif_cv_image_t frame, fg_mask;
    eif_cv_image_create(&frame, IMG_WIDTH, IMG_HEIGHT, EIF_CV_GRAY8, &pool);
    eif_cv_image_create(&fg_mask, IMG_WIDTH, IMG_HEIGHT, EIF_CV_GRAY8, &pool);
    
    // --- Background Subtraction ---
    if (!json_mode) {
        printf("--- Background Subtraction + Tracking ---\n\n");
    }
    
    eif_cv_bg_model_t bg_model;
    eif_cv_bg_init(&bg_model, IMG_WIDTH, IMG_HEIGHT, 0.1f, &pool);
    
    // Train background model
    generate_frame(&frame, 0);
    for (int i = 0; i < 20; i++) {
        eif_cv_bg_update(&bg_model, &frame, &fg_mask, 2.5f);
    }
    
    if (!json_mode) {
        eif_cv_write_pgm(&frame, "output/01_background.pgm");
        printf("Saved: output/01_background.pgm (training frame)\n");
    }
    
    // --- Multi-Object Tracker ---
    eif_cv_tracker_t tracker;
    eif_cv_tracker_init(&tracker, 10, 5, 2, 0.3f, &pool);
    
    float total_error = 0;
    int error_count = 0;
    
    // Process frames
    for (int t = 1; t <= num_frames; t++) {
        generate_frame(&frame, t);
        eif_cv_bg_update(&bg_model, &frame, &fg_mask, 2.5f);
        
        int fg_pixels = count_fg_pixels(&fg_mask);
        
        // Detection
        eif_cv_rect_t det = {10 + t * 5, 10 + t * 4, 20, 20};
        eif_cv_tracker_update(&tracker, &det, 1);
        
        // Get tracks
        eif_cv_track_t tracks[5];
        int num_tracks = eif_cv_tracker_get_tracks(&tracker, tracks, 5);
        
        if (num_tracks > 0) {
            // Get ground truth
            float true_x, true_y;
            get_object_center(t, &true_x, &true_y);
            
            float pred_x = tracks[0].center.x;
            float pred_y = tracks[0].center.y;
            float error = (pred_x - true_x) * (pred_x - true_x) + 
                          (pred_y - true_y) * (pred_y - true_y);
            error = error > 0 ? sqrtf(error) : 0;
            
            total_error += error;
            error_count++;
            
            if (json_mode) {
                output_json_track(t, tracks[0].id, true_x, true_y, pred_x, pred_y, fg_pixels);
            } else {
                if (t <= 10 || t % 5 == 0) {
                    printf("Frame %2d: Track ID=%d at (%.0f,%.0f) pred=(%.0f,%.0f) err=%.1f fg=%d\n",
                           t, tracks[0].id, true_x, true_y, pred_x, pred_y, error, fg_pixels);
                }
            }
        }
        
        // Save some frames
        if (!json_mode && t <= 5) {
            char fname[64];
            snprintf(fname, sizeof(fname), "output/%02d_frame.pgm", t + 1);
            eif_cv_write_pgm(&frame, fname);
        }
    }
    
    float avg_error = error_count > 0 ? total_error / error_count : 0;
    
    if (json_mode) {
        printf("{\"type\": \"summary\", \"frames\": %d, \"avg_tracking_error\": %.2f}\n",
               num_frames, avg_error);
    } else {
        printf("\n--- Summary ---\n");
        printf("Frames processed: %d\n", num_frames);
        printf("Average tracking error: %.2f pixels\n", avg_error);
        printf("Generated images in output/ directory\n\n");
        
        printf("Visualization:\n");
        printf("  python3 tools/cv_visualizer.py --grid output/*.pgm\n");
    }
    
    return 0;
}
