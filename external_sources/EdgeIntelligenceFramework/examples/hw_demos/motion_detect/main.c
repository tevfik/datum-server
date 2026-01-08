/**
 * @file main.c
 * @brief Motion Detection Demo for ESP32-CAM
 * 
 * Frame differencing for motion detection with object counting.
 * 
 * Features:
 * - Background subtraction
 * - Motion blob detection
 * - Entry/exit counting
 * - Activity level scoring
 * - JSON output for visualization
 * 
 * Usage:
 *   ./motion_detect_demo                          # Interactive
 *   ./motion_detect_demo --json                   # JSON for plotter
 *   ./motion_detect_demo --json | python3 tools/eif_plotter.py --stdin
 * 
 * Target: ESP32-CAM / ESP32-S3-CAM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>

#include "eif_cv.h"
#include "eif_memory.h"

#define WIDTH 80
#define HEIGHT 60
#define MOTION_THRESHOLD 30
#define MIN_BLOB_SIZE 20

static uint8_t pool_buffer[128 * 1024];
static eif_memory_pool_t pool;

static bool json_mode = false;
static bool continuous_mode = false;
static int sample_count = 0;

// Simple background model
typedef struct {
    float background[WIDTH * HEIGHT];
    float alpha;
} bg_model_t;

static void bg_model_init(bg_model_t* bg, const uint8_t* first_frame) {
    bg->alpha = 0.05f;
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        bg->background[i] = (float)first_frame[i];
    }
}

static void bg_model_update(bg_model_t* bg, const uint8_t* frame) {
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        bg->background[i] = bg->alpha * frame[i] + (1 - bg->alpha) * bg->background[i];
    }
}

static int detect_motion(const bg_model_t* bg, const uint8_t* frame, uint8_t* mask) {
    int motion_pixels = 0;
    
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        float diff = fabsf(frame[i] - bg->background[i]);
        if (diff > MOTION_THRESHOLD) {
            mask[i] = 255;
            motion_pixels++;
        } else {
            mask[i] = 0;
        }
    }
    
    return motion_pixels;
}

static int count_blobs(const uint8_t* mask, int w, int h) {
    uint8_t* visited = calloc(w * h, 1);
    if (!visited) return 0;
    
    int blob_count = 0;
    
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            if (mask[idx] > 0 && !visited[idx]) {
                int blob_size = 0;
                int stack[1000];
                int sp = 0;
                
                stack[sp++] = idx;
                
                while (sp > 0 && sp < 999) {
                    int curr = stack[--sp];
                    if (curr < 0 || curr >= w * h) continue;
                    if (visited[curr] || mask[curr] == 0) continue;
                    
                    visited[curr] = 1;
                    blob_size++;
                    
                    int cx = curr % w, cy = curr / w;
                    if (cx > 0) stack[sp++] = curr - 1;
                    if (cx < w-1) stack[sp++] = curr + 1;
                    if (cy > 0) stack[sp++] = curr - w;
                    if (cy < h-1) stack[sp++] = curr + w;
                }
                
                if (blob_size >= MIN_BLOB_SIZE) {
                    blob_count++;
                }
            }
        }
    }
    
    free(visited);
    return blob_count;
}

static void generate_frame(uint8_t* frame, int w, int h, int time_step, int* obj_x, int* obj_y) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            frame[y * w + x] = 50 + (rand() % 10);
        }
    }
    
    *obj_x = (time_step * 2) % (w + 20) - 10;
    *obj_y = h / 2 + (int)(10 * sinf(time_step * 0.2f));
    
    int obj_r = 8;
    for (int dy = -obj_r; dy <= obj_r; dy++) {
        for (int dx = -obj_r; dx <= obj_r; dx++) {
            int px = *obj_x + dx;
            int py = *obj_y + dy;
            if (px >= 0 && px < w && py >= 0 && py < h) {
                if (dx*dx + dy*dy <= obj_r*obj_r) {
                    frame[py * w + px] = 180;
                }
            }
        }
    }
}

// ============================================================================
// JSON Output
// ============================================================================

static void output_json(int frame_num, float motion_pct, int blobs, 
                        int obj_x, int obj_y, const char* activity) {
    printf("{\"timestamp\": %d, \"type\": \"motion\"", sample_count++);
    printf(", \"signals\": {\"motion_percent\": %.2f, \"blobs\": %d}", motion_pct, blobs);
    printf(", \"object\": {\"x\": %d, \"y\": %d}", obj_x, obj_y);
    printf(", \"state\": {\"activity_level\": \"%s\"}", activity);
    printf(", \"detection\": %s", motion_pct > 2.0f ? "true" : "false");
    printf("}\n");
    fflush(stdout);
}

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  --json        Output JSON for real-time plotting\n");
    printf("  --continuous  Run without pauses\n");
    printf("  --frames N    Number of frames to process (default: 50)\n");
    printf("  --help        Show this help\n");
    printf("\nExamples:\n");
    printf("  %s --json | python3 tools/eif_plotter.py --stdin\n", prog);
    printf("  %s --continuous --frames 200\n", prog);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    int num_frames = 50;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "--continuous") == 0) {
            continuous_mode = true;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            num_frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    srand(time(NULL));
    
    if (!json_mode) {
        printf("\n");
        printf("+========================================================+\n");
        printf("|    Motion Detection Demo (ESP32-CAM Simulation)        |\n");
        printf("+========================================================+\n\n");
    }
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    uint8_t* frame = eif_memory_alloc(&pool, WIDTH * HEIGHT, 1);
    uint8_t* mask = eif_memory_alloc(&pool, WIDTH * HEIGHT, 1);
    bg_model_t bg;
    
    int obj_x, obj_y;
    generate_frame(frame, WIDTH, HEIGHT, 0, &obj_x, &obj_y);
    bg_model_init(&bg, frame);
    
    if (!json_mode) {
        printf("Resolution: %dx%d\n", WIDTH, HEIGHT);
        printf("Motion threshold: %d gray levels\n\n", MOTION_THRESHOLD);
        
        printf("+---------+------------+---------+---------------+\n");
        printf("|  Frame  | Motion %%   |  Blobs  |    Activity   |\n");
        printf("+---------+------------+---------+---------------+\n");
    }
    
    int total_entries = 0;
    int prev_blobs = 0;
    
    for (int t = 1; t <= num_frames; t++) {
        generate_frame(frame, WIDTH, HEIGHT, t, &obj_x, &obj_y);
        
        int motion_pixels = detect_motion(&bg, frame, mask);
        float motion_pct = 100.0f * motion_pixels / (WIDTH * HEIGHT);
        
        int blobs = count_blobs(mask, WIDTH, HEIGHT);
        
        if (blobs > prev_blobs) {
            total_entries += (blobs - prev_blobs);
        }
        prev_blobs = blobs;
        
        const char* activity;
        if (motion_pct > 10) activity = "HIGH";
        else if (motion_pct > 2) activity = "MEDIUM";
        else if (motion_pct > 0.5) activity = "LOW";
        else activity = "NONE";
        
        if (json_mode) {
            output_json(t, motion_pct, blobs, obj_x, obj_y, activity);
        } else {
            if (t <= 10 || t % 10 == 0 || motion_pct > 5) {
                printf("|   %3d   |   %5.1f%%   |    %d    |   %-10s  |\n",
                       t, motion_pct, blobs, activity);
            }
        }
        
        bg_model_update(&bg, frame);
    }
    
    if (!json_mode) {
        printf("+---------+------------+---------+---------------+\n");
        printf("\nTotal entries detected: %d\n\n", total_entries);
        
        printf("+--------------------------------------------------------+\n");
        printf("|  For ESP32-CAM:                                        |\n");
        printf("|  1. Use esp_camera for frame capture                   |\n");
        printf("|  2. Trigger GPIO on motion (alarm, light)              |\n");
        printf("|  3. Save clips to SD card on detection                 |\n");
        printf("|  4. Push notification via MQTT/HTTP                    |\n");
        printf("+--------------------------------------------------------+\n\n");
    } else {
        printf("{\"type\": \"summary\", \"total_entries\": %d, \"frames_processed\": %d}\n",
               total_entries, num_frames);
    }
    
    return 0;
}
