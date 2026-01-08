/**
 * @file main.c
 * @brief Smart Doorbell Project - Complete ESP32-CAM Application
 * 
 * End-to-end smart doorbell combining:
 * - Motion detection (camera)
 * - Person detection (ML)
 * - Audio alert detection (optional)
 * - WiFi notification (MQTT/HTTP)
 * - Image capture and upload
 * 
 * Target: ESP32-CAM / ESP32-S3-CAM
 * 
 * This is a COMPLETE project template demonstrating
 * how to combine multiple EIF modules into a real application.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "eif_cv.h"
#include "eif_memory.h"

// ============================================================================
// Configuration
// ============================================================================

#define FRAME_WIDTH  160
#define FRAME_HEIGHT 120
#define MOTION_THRESHOLD 25
#define PERSON_CONFIDENCE_THRESHOLD 0.6f
#define COOLDOWN_SECONDS 5

// ============================================================================
// State Machine
// ============================================================================

typedef enum {
    STATE_IDLE,
    STATE_MOTION_DETECTED,
    STATE_ANALYZING,
    STATE_ALERTING,
    STATE_COOLDOWN
} doorbell_state_t;

typedef struct {
    doorbell_state_t state;
    int motion_frames;
    int cooldown_counter;
    int total_alerts;
    int total_visitors;
    float last_confidence;
} doorbell_t;

// ============================================================================
// Simulated Components
// ============================================================================

static uint8_t pool_buffer[128 * 1024];
static eif_memory_pool_t pool;

// Background model
static float background[FRAME_WIDTH * FRAME_HEIGHT];
static int bg_initialized = 0;

// Simulated person detector (would use CNN in real implementation)
static float detect_person(const uint8_t* frame, int w, int h) {
    // Simple heuristic: look for vertical blob in center
    float center_mass = 0;
    float total_mass = 0;
    
    for (int y = h/4; y < 3*h/4; y++) {
        for (int x = w/3; x < 2*w/3; x++) {
            center_mass += frame[y * w + x];
        }
    }
    center_mass /= ((h/2) * (w/3));
    
    for (int i = 0; i < w * h; i++) {
        total_mass += frame[i];
    }
    total_mass /= (w * h);
    
    // If center is significantly different from average, might be a person
    float diff = fabsf(center_mass - total_mass) / 255.0f;
    return diff * 2.0f;  // Scale to 0-1 range
}

static int detect_motion(const uint8_t* frame, int w, int h) {
    if (!bg_initialized) {
        for (int i = 0; i < w * h; i++) {
            background[i] = frame[i];
        }
        bg_initialized = 1;
        return 0;
    }
    
    int motion_pixels = 0;
    for (int i = 0; i < w * h; i++) {
        float diff = fabsf(frame[i] - background[i]);
        if (diff > MOTION_THRESHOLD) {
            motion_pixels++;
        }
        // Update background slowly
        background[i] = 0.95f * background[i] + 0.05f * frame[i];
    }
    
    return motion_pixels;
}

// Generate simulated camera frame
static void capture_frame(uint8_t* frame, int w, int h, int time_step, int has_visitor) {
    // Background
    for (int i = 0; i < w * h; i++) {
        frame[i] = 40 + (rand() % 10);
    }
    
    // Add a person-like shape if visitor present
    if (has_visitor) {
        int cx = w/2 + (time_step % 20) - 10;
        int cy = h/2;
        
        // Head
        for (int dy = -10; dy < 5; dy++) {
            for (int dx = -8; dx < 8; dx++) {
                int px = cx + dx, py = cy + dy - 20;
                if (px >= 0 && px < w && py >= 0 && py < h) {
                    if (dx*dx + dy*dy < 64) {
                        frame[py * w + px] = 150;
                    }
                }
            }
        }
        
        // Body
        for (int dy = 5; dy < 40; dy++) {
            for (int dx = -15; dx < 15; dx++) {
                int px = cx + dx, py = cy + dy - 20;
                if (px >= 0 && px < w && py >= 0 && py < h) {
                    frame[py * w + px] = 140;
                }
            }
        }
    }
}

// Simulated notification
static void send_notification(const char* message, float confidence) {
    printf("    📱 NOTIFICATION: %s (conf: %.1f%%)\n", message, confidence * 100);
}

static void capture_and_upload_image(int frame_num) {
    printf("    📷 Image captured and uploaded (frame_%d.jpg)\n", frame_num);
}

// ============================================================================
// State Machine Logic
// ============================================================================

static const char* state_names[] = {
    "IDLE", "MOTION_DETECTED", "ANALYZING", "ALERTING", "COOLDOWN"
};

static void doorbell_init(doorbell_t* db) {
    db->state = STATE_IDLE;
    db->motion_frames = 0;
    db->cooldown_counter = 0;
    db->total_alerts = 0;
    db->total_visitors = 0;
    db->last_confidence = 0;
}

static void doorbell_update(doorbell_t* db, const uint8_t* frame, int w, int h, int time_step) {
    int motion = detect_motion(frame, w, h);
    float motion_pct = 100.0f * motion / (w * h);
    float person_conf = 0;
    
    switch (db->state) {
        case STATE_IDLE:
            if (motion_pct > 2.0f) {
                db->state = STATE_MOTION_DETECTED;
                db->motion_frames = 1;
                printf("  [%3d] Motion detected! (%.1f%% pixels)\n", time_step, motion_pct);
            }
            break;
            
        case STATE_MOTION_DETECTED:
            if (motion_pct > 1.0f) {
                db->motion_frames++;
                if (db->motion_frames >= 3) {
                    db->state = STATE_ANALYZING;
                    printf("  [%3d] Sustained motion, analyzing...\n", time_step);
                }
            } else {
                db->state = STATE_IDLE;
                db->motion_frames = 0;
            }
            break;
            
        case STATE_ANALYZING:
            person_conf = detect_person(frame, w, h);
            db->last_confidence = person_conf;
            
            if (person_conf > PERSON_CONFIDENCE_THRESHOLD) {
                db->state = STATE_ALERTING;
                db->total_visitors++;
                printf("  [%3d] Person detected! Confidence: %.1f%%\n", time_step, person_conf * 100);
            } else {
                db->state = STATE_IDLE;
                printf("  [%3d] No person (conf: %.1f%%), returning to idle\n", time_step, person_conf * 100);
            }
            break;
            
        case STATE_ALERTING:
            send_notification("Visitor at door!", db->last_confidence);
            capture_and_upload_image(time_step);
            db->total_alerts++;
            db->state = STATE_COOLDOWN;
            db->cooldown_counter = COOLDOWN_SECONDS;
            break;
            
        case STATE_COOLDOWN:
            db->cooldown_counter--;
            if (db->cooldown_counter <= 0) {
                db->state = STATE_IDLE;
                printf("  [%3d] Cooldown complete, resuming monitoring\n", time_step);
            }
            break;
    }
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    srand(time(NULL));
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║      Smart Doorbell Project - Complete Application Demo        ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    uint8_t* frame = eif_memory_alloc(&pool, FRAME_WIDTH * FRAME_HEIGHT, 1);
    doorbell_t doorbell;
    doorbell_init(&doorbell);
    
    printf("Configuration:\n");
    printf("  Resolution: %dx%d\n", FRAME_WIDTH, FRAME_HEIGHT);
    printf("  Motion threshold: %d\n", MOTION_THRESHOLD);
    printf("  Person confidence: %.0f%%\n", PERSON_CONFIDENCE_THRESHOLD * 100);
    printf("  Cooldown: %d seconds\n\n", COOLDOWN_SECONDS);
    
    printf("Simulation: 60 seconds, visitor at t=20s and t=45s\n\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    
    for (int t = 0; t < 60; t++) {
        // Simulate visitor at certain times
        int has_visitor = (t >= 20 && t <= 25) || (t >= 45 && t <= 48);
        
        capture_frame(frame, FRAME_WIDTH, FRAME_HEIGHT, t, has_visitor);
        doorbell_update(&doorbell, frame, FRAME_WIDTH, FRAME_HEIGHT, t);
    }
    
    printf("─────────────────────────────────────────────────────────────────\n\n");
    
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│  SESSION SUMMARY                                                │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    printf("│  Total visitors detected:  %d                                   │\n", doorbell.total_visitors);
    printf("│  Total alerts sent:        %d                                   │\n", doorbell.total_alerts);
    printf("│  Images captured:          %d                                   │\n", doorbell.total_alerts);
    printf("└─────────────────────────────────────────────────────────────────┘\n\n");
    
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│  FOR ESP32-CAM DEPLOYMENT:                                      │\n");
    printf("│                                                                 │\n");
    printf("│  1. Add esp_camera for real frame capture                       │\n");
    printf("│  2. Replace detect_person() with CNN model (MobileNet)          │\n");
    printf("│  3. Add WiFi + MQTT for push notifications                      │\n");
    printf("│  4. Add HTTP image upload to cloud storage                      │\n");
    printf("│  5. Add PIR sensor for initial trigger (power saving)           │\n");
    printf("│  6. Add SD card logging                                         │\n");
    printf("│  7. Add button for manual doorbell ring                         │\n");
    printf("└─────────────────────────────────────────────────────────────────┘\n\n");
    
    return 0;
}
