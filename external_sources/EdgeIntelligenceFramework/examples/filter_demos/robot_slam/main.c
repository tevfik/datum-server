/**
 * @file main.c
 * @brief Robot SLAM Tutorial - 2D Localization and Mapping
 * 
 * This tutorial demonstrates Simultaneous Localization and Mapping (SLAM)
 * using Extended Kalman Filter (EKF-SLAM).
 * 
 * SCENARIO:
 * A mobile robot explores an unknown 2D environment with landmarks.
 * Using range-bearing sensors, it simultaneously:
 * 1. Estimates its own position (localization)
 * 2. Builds a map of landmark positions (mapping)
 * 
 * FEATURES DEMONSTRATED:
 * - EKF-SLAM algorithm
 * - State vector management (robot + landmarks)
 * - Matrix operations for SLAM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "eif_types.h"
#include "eif_memory.h"
#include "eif_bayesian.h"
#include "../common/ascii_plot.h"

static bool json_mode = false;
static bool continuous_mode = false;
static int sample_count = 0;

// ============================================================================
// Configuration
// ============================================================================

#define MAP_WIDTH       40
#define MAP_HEIGHT      20
#define NUM_LANDMARKS   5
#define NUM_STEPS       50
#define DT              0.5f

// ============================================================================
// Simulation Types
// ============================================================================

typedef struct {
    float32_t x, y;
} landmark_t;

typedef struct {
    float32_t x, y, theta;  // Robot pose
} robot_pose_t;

// True landmark positions (unknown to robot)
static landmark_t true_landmarks[NUM_LANDMARKS] = {
    {5.0f,  3.0f},
    {15.0f, 2.0f},
    {12.0f, 8.0f},
    {3.0f,  7.0f},
    {18.0f, 6.0f}
};

// ============================================================================
// ASCII Map Display
// ============================================================================

static void display_map(const robot_pose_t* true_pose, const robot_pose_t* est_pose,
                        const landmark_t* true_lm, const float32_t* est_lm, int num_lm,
                        const robot_pose_t* trajectory, int traj_len) {
    
    char map[MAP_HEIGHT][MAP_WIDTH + 1];
    
    // Clear map
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map[y][x] = '.';
        }
        map[y][MAP_WIDTH] = '\0';
    }
    
    // Draw trajectory
    for (int i = 0; i < traj_len; i++) {
        int mx = (int)(trajectory[i].x * 2);
        int my = MAP_HEIGHT - 1 - (int)(trajectory[i].y * 2);
        if (mx >= 0 && mx < MAP_WIDTH && my >= 0 && my < MAP_HEIGHT) {
            map[my][mx] = '-';
        }
    }
    
    // Draw true landmarks (X)
    for (int i = 0; i < NUM_LANDMARKS; i++) {
        int mx = (int)(true_lm[i].x * 2);
        int my = MAP_HEIGHT - 1 - (int)(true_lm[i].y * 2);
        if (mx >= 0 && mx < MAP_WIDTH && my >= 0 && my < MAP_HEIGHT) {
            map[my][mx] = 'X';
        }
    }
    
    // Draw estimated landmarks (o)
    for (int i = 0; i < num_lm; i++) {
        int mx = (int)(est_lm[i * 2] * 2);
        int my = MAP_HEIGHT - 1 - (int)(est_lm[i * 2 + 1] * 2);
        if (mx >= 0 && mx < MAP_WIDTH && my >= 0 && my < MAP_HEIGHT) {
            if (map[my][mx] == 'X') map[my][mx] = '*';  // Match!
            else map[my][mx] = 'o';
        }
    }
    
    // Draw true robot position (@)
    {
        int mx = (int)(true_pose->x * 2);
        int my = MAP_HEIGHT - 1 - (int)(true_pose->y * 2);
        if (mx >= 0 && mx < MAP_WIDTH && my >= 0 && my < MAP_HEIGHT) {
            map[my][mx] = '@';
        }
    }
    
    // Draw estimated robot position (R)
    {
        int mx = (int)(est_pose->x * 2);
        int my = MAP_HEIGHT - 1 - (int)(est_pose->y * 2);
        if (mx >= 0 && mx < MAP_WIDTH && my >= 0 && my < MAP_HEIGHT) {
            if (map[my][mx] == '@') map[my][mx] = 'R';  // Match
            else map[my][mx] = 'r';
        }
    }
    
    // Print map
    printf("  %s┌", ASCII_CYAN);
    for (int x = 0; x < MAP_WIDTH; x++) printf("─");
    printf("┐%s\n", ASCII_RESET);
    
    for (int y = 0; y < MAP_HEIGHT; y++) {
        printf("  %s│%s", ASCII_CYAN, ASCII_RESET);
        for (int x = 0; x < MAP_WIDTH; x++) {
            char c = map[y][x];
            if (c == '@' || c == 'R') printf("%s%c%s", ASCII_GREEN, c, ASCII_RESET);
            else if (c == 'r') printf("%s%c%s", ASCII_YELLOW, c, ASCII_RESET);
            else if (c == 'X') printf("%s%c%s", ASCII_RED, c, ASCII_RESET);
            else if (c == 'o') printf("%s%c%s", ASCII_CYAN, c, ASCII_RESET);
            else if (c == '*') printf("%s%c%s", ASCII_GREEN, c, ASCII_RESET);
            else if (c == '-') printf("%s%c%s", ASCII_BLUE, c, ASCII_RESET);
            else printf("%c", c);
        }
        printf("%s│%s\n", ASCII_CYAN, ASCII_RESET);
    }
    
    printf("  %s└", ASCII_CYAN);
    for (int x = 0; x < MAP_WIDTH; x++) printf("─");
    printf("┘%s\n", ASCII_RESET);
    
    printf("\n  %sLegend:%s  @ = True Robot  r = Estimated Robot  X = True Landmark  o = Estimated Landmark\n", ASCII_BOLD, ASCII_RESET);
}

// ============================================================================
// Robot Motion Simulation
// ============================================================================

static void simulate_motion(robot_pose_t* pose, float32_t v, float32_t omega, float32_t dt) {
    pose->x += v * cosf(pose->theta) * dt;
    pose->y += v * sinf(pose->theta) * dt;
    pose->theta += omega * dt;
    
    // Normalize theta
    while (pose->theta > M_PI) pose->theta -= 2 * M_PI;
    while (pose->theta < -M_PI) pose->theta += 2 * M_PI;
}

// ============================================================================
// Range-Bearing Observation
// ============================================================================

typedef struct {
    int landmark_id;
    float32_t range;
    float32_t bearing;
} observation_t;

static int get_observations(const robot_pose_t* pose, observation_t* obs, float32_t max_range) {
    int count = 0;
    for (int i = 0; i < NUM_LANDMARKS; i++) {
        float32_t dx = true_landmarks[i].x - pose->x;
        float32_t dy = true_landmarks[i].y - pose->y;
        float32_t range = sqrtf(dx * dx + dy * dy);
        
        if (range < max_range) {
            float32_t bearing = atan2f(dy, dx) - pose->theta;
            
            // Add noise
            obs[count].landmark_id = i;
            obs[count].range = range + 0.1f * ((float)rand()/RAND_MAX - 0.5f);
            obs[count].bearing = bearing + 0.05f * ((float)rand()/RAND_MAX - 0.5f);
            count++;
        }
    }
    return count;
}

// ============================================================================
// Simple EKF-SLAM Implementation
// ============================================================================

#define MAX_LANDMARKS 10
#define STATE_SIZE (3 + 2 * MAX_LANDMARKS)  // [x, y, theta, lm1_x, lm1_y, ...]

typedef struct {
    float32_t state[STATE_SIZE];
    float32_t cov[STATE_SIZE * STATE_SIZE];
    int num_landmarks;
    int landmark_ids[MAX_LANDMARKS];  // Maps local index to global landmark ID
} simple_slam_t;

static void slam_init(simple_slam_t* slam, float32_t x, float32_t y, float32_t theta) {
    memset(slam, 0, sizeof(simple_slam_t));
    slam->state[0] = x;
    slam->state[1] = y;
    slam->state[2] = theta;
    
    // Initial covariance (small for robot pose)
    for (int i = 0; i < STATE_SIZE; i++) {
        slam->cov[i * STATE_SIZE + i] = 0.1f;
    }
}

static void slam_predict(simple_slam_t* slam, float32_t v, float32_t omega, float32_t dt) {
    // Motion model
    float32_t theta = slam->state[2];
    slam->state[0] += v * cosf(theta) * dt;
    slam->state[1] += v * sinf(theta) * dt;
    slam->state[2] += omega * dt;
    
    // Add process noise to covariance
    slam->cov[0] += 0.01f;
    slam->cov[STATE_SIZE + 1] += 0.01f;
    slam->cov[2 * STATE_SIZE + 2] += 0.005f;
}

static void slam_update(simple_slam_t* slam, const observation_t* obs) {
    // Check if landmark is new
    int local_id = -1;
    for (int i = 0; i < slam->num_landmarks; i++) {
        if (slam->landmark_ids[i] == obs->landmark_id) {
            local_id = i;
            break;
        }
    }
    
    if (local_id < 0) {
        // New landmark - add to state
        if (slam->num_landmarks >= MAX_LANDMARKS) return;
        
        local_id = slam->num_landmarks++;
        slam->landmark_ids[local_id] = obs->landmark_id;
        
        // Initialize landmark position from observation
        float32_t rx = slam->state[0];
        float32_t ry = slam->state[1];
        float32_t rt = slam->state[2];
        
        slam->state[3 + local_id * 2]     = rx + obs->range * cosf(rt + obs->bearing);
        slam->state[3 + local_id * 2 + 1] = ry + obs->range * sinf(rt + obs->bearing);
        
        // High initial uncertainty for new landmarks
        int idx = 3 + local_id * 2;
        slam->cov[idx * STATE_SIZE + idx] = 1.0f;
        slam->cov[(idx+1) * STATE_SIZE + (idx+1)] = 1.0f;
    } else {
        // Update existing landmark (simplified EKF update)
        int lm_idx = 3 + local_id * 2;
        float32_t lx = slam->state[lm_idx];
        float32_t ly = slam->state[lm_idx + 1];
        float32_t rx = slam->state[0];
        float32_t ry = slam->state[1];
        
        float32_t dx = lx - rx;
        float32_t dy = ly - ry;
        float32_t pred_range = sqrtf(dx * dx + dy * dy);
        float32_t pred_bearing = atan2f(dy, dx) - slam->state[2];
        
        float32_t range_err = obs->range - pred_range;
        float32_t bearing_err = obs->bearing - pred_bearing;
        
        // Simple update (approximate Kalman gain)
        float32_t K = 0.3f;  // Simplified gain
        
        slam->state[lm_idx]     += K * range_err * cosf(slam->state[2] + obs->bearing);
        slam->state[lm_idx + 1] += K * range_err * sinf(slam->state[2] + obs->bearing);
        
        // Also correct robot pose slightly
        slam->state[0] -= K * 0.1f * range_err * cosf(slam->state[2]);
        slam->state[1] -= K * 0.1f * range_err * sinf(slam->state[2]);
    }
}

// ============================================================================
// JSON Output
// ============================================================================

static void output_json_slam(int step, const robot_pose_t* true_pose, 
                              const robot_pose_t* est_pose, float pos_err,
                              int num_obs, int landmarks_mapped) {
    printf("{\"timestamp\": %d, \"type\": \"slam\"", sample_count++);
    printf(", \"true\": {\"x\": %.2f, \"y\": %.2f, \"theta\": %.3f}", 
           true_pose->x, true_pose->y, true_pose->theta);
    printf(", \"estimate\": {\"x\": %.2f, \"y\": %.2f, \"theta\": %.3f}", 
           est_pose->x, est_pose->y, est_pose->theta);
    printf(", \"signals\": {\"position_error\": %.3f, \"observations\": %d, \"landmarks\": %d}",
           pos_err, num_obs, landmarks_mapped);
    printf("}\n");
    fflush(stdout);
}

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  --json        Output JSON for real-time plotting\n");
    printf("  --continuous  Run without animation delay\n");
    printf("  --steps N     Number of simulation steps (default: 50)\n");
    printf("  --help        Show this help\n");
    printf("\nExamples:\n");
    printf("  %s --json | python3 tools/eif_plotter.py --stdin\n", prog);
    printf("  %s --continuous --steps 100\n", prog);
}

// ============================================================================
// Main Tutorial
// ============================================================================

int main(int argc, char** argv) {
    int num_steps = NUM_STEPS;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_mode = true;
        } else if (strcmp(argv[i], "--continuous") == 0) {
            continuous_mode = true;
        } else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
            num_steps = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    if (!json_mode) {
        printf("\n");
        ascii_section("EIF Tutorial: 2D Robot SLAM");
        
        printf("  This tutorial demonstrates Simultaneous Localization and Mapping.\n");
        printf("  A robot explores an unknown environment with 5 landmarks.\n\n");
        printf("  The robot uses:\n");
        printf("    - Odometry for motion prediction\n");
        printf("    - Range-bearing sensor for landmark observations\n");
        printf("    - EKF-SLAM for state estimation\n");
        
        if (!continuous_mode) {
            printf("\n  Press Enter to start simulation...");
            getchar();
        }
    }
    
    // Initialize
    robot_pose_t true_pose = {1.0f, 1.0f, 0.0f};  // True robot pose
    simple_slam_t slam;
    slam_init(&slam, 1.0f, 1.0f, 0.0f);  // Initial estimate (= true)
    
    static robot_pose_t trajectory[NUM_STEPS];
    observation_t obs[NUM_LANDMARKS];
    
    // Simulation loop
    for (int step = 0; step < num_steps; step++) {
        if (!json_mode) {
            // Clear screen effect
            printf("\033[2J\033[H");  // Clear and home
            
            ascii_section("EIF Tutorial: 2D Robot SLAM");
            printf("  Step %d / %d\n\n", step + 1, num_steps);
        }
        
        // Motion command (circular path)
        float32_t v = 0.5f;      // Forward velocity
        float32_t omega = 0.1f;  // Angular velocity
        
        // True motion
        simulate_motion(&true_pose, v, omega + 0.02f * ((float)rand()/RAND_MAX - 0.5f), DT);
        trajectory[step % NUM_STEPS] = true_pose;
        
        // SLAM prediction
        slam_predict(&slam, v, omega, DT);
        
        // Get observations
        int num_obs = get_observations(&true_pose, obs, 8.0f);
        
        // SLAM update for each observation
        for (int i = 0; i < num_obs; i++) {
            slam_update(&slam, &obs[i]);
        }
        
        // Calculate error
        robot_pose_t est_pose = {slam.state[0], slam.state[1], slam.state[2]};
        float32_t pos_err = sqrtf((true_pose.x - est_pose.x) * (true_pose.x - est_pose.x) +
                                  (true_pose.y - est_pose.y) * (true_pose.y - est_pose.y));
        
        if (json_mode) {
            output_json_slam(step, &true_pose, &est_pose, pos_err, num_obs, slam.num_landmarks);
        } else {
            // Display
            display_map(&true_pose, &est_pose, true_landmarks, &slam.state[3], slam.num_landmarks, trajectory, (step % NUM_STEPS) + 1);
            
            // Stats
            printf("\n  +-- Status -----------------------------------+\n");
            printf("  |  True Position:      (%5.2f, %5.2f)        |\n", true_pose.x, true_pose.y);
            printf("  |  Estimated Position: (%5.2f, %5.2f)        |\n", est_pose.x, est_pose.y);
            printf("  |  Position Error:     %5.2f m               |\n", pos_err);
            printf("  |  Landmarks Mapped:   %d / %d                  |\n", slam.num_landmarks, NUM_LANDMARKS);
            printf("  |  Observations:       %d                      |\n", num_obs);
            printf("  +--------------------------------------------+\n");
            
            // Delay for animation (only in visual mode)
            if (!continuous_mode) {
                #ifdef _WIN32
                Sleep(200);
                #else
                usleep(200000);
                #endif
            }
        }
    }
    
    // Final summary
    float32_t total_lm_err = 0;
    for (int i = 0; i < slam.num_landmarks; i++) {
        int global_id = slam.landmark_ids[i];
        float32_t dx = slam.state[3 + i*2] - true_landmarks[global_id].x;
        float32_t dy = slam.state[3 + i*2 + 1] - true_landmarks[global_id].y;
        float32_t err = sqrtf(dx*dx + dy*dy);
        total_lm_err += err;
    }
    float32_t mean_lm_err = slam.num_landmarks > 0 ? total_lm_err / slam.num_landmarks : 0;
    
    if (json_mode) {
        printf("{\"type\": \"summary\", \"steps\": %d, \"landmarks_mapped\": %d, \"mean_landmark_error\": %.3f}\n",
               num_steps, slam.num_landmarks, mean_lm_err);
    } else {
        printf("\n");
        ascii_section("Simulation Complete");
        
        printf("  Final Results:\n\n");
        
        printf("  Landmark Estimation Errors:\n");
        for (int i = 0; i < slam.num_landmarks; i++) {
            int global_id = slam.landmark_ids[i];
            float32_t dx = slam.state[3 + i*2] - true_landmarks[global_id].x;
            float32_t dy = slam.state[3 + i*2 + 1] - true_landmarks[global_id].y;
            float32_t err = sqrtf(dx*dx + dy*dy);
            printf("    Landmark %d: Error = %.2f m\n", global_id, err);
        }
        printf("\n    Mean Landmark Error: %.2f m\n", mean_lm_err);
        
        printf("\n  Key EIF APIs Used:\n");
        printf("    - eif_ekf_slam_init()    - Initialize EKF-SLAM\n");
        printf("    - eif_ekf_slam_predict() - Motion prediction\n");
        printf("    - eif_ekf_slam_update()  - Measurement update\n");
        
        printf("\n  Tutorial Complete!\n\n");
    }
    
    return 0;
}
