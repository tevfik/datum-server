/**
 * @file main.c
 * @brief IMU Fusion Demo - EKF-based Sensor Fusion
 * 
 * Demonstrates the eif_imu module using an Extended Kalman Filter (EKF)
 * to fuse IMU (accelerometer + gyroscope), GPS, and barometer data.
 * 
 * Features:
 * - 15-state EKF for robust state estimation
 * - Simulated quadcopter flight trajectory
 * - GPS with realistic noise and update rate
 * - Barometer altitude aiding
 * - Visualization of position, velocity, and attitude
 * - JSON output for real-time plotting (--json flag)
 * 
 * Usage:
 *   ./imu_fusion_demo           # Normal output
 *   ./imu_fusion_demo --json    # JSON output for visualization
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include "eif_imu.h"
#include "eif_memory.h"

// =============================================================================
// Demo Configuration
// =============================================================================

#define DEMO_DURATION_SEC  30.0f
#define IMU_RATE_HZ        100.0f
#define GPS_RATE_HZ        5.0f
#define BARO_RATE_HZ       25.0f

// Output mode
static bool json_output = false;

#define PI 3.14159265358979f
#define DEG2RAD (PI / 180.0f)
#define RAD2DEG (180.0f / PI)

// Noise parameters
#define ACCEL_NOISE_STD    0.5f   // m/s^2
#define GYRO_NOISE_STD     0.01f  // rad/s
#define GPS_POS_NOISE_STD  2.5f   // meters
#define GPS_VEL_NOISE_STD  0.1f   // m/s
#define BARO_NOISE_STD     1.0f   // meters

// =============================================================================
// Utility Functions
// =============================================================================

static float random_gaussian(void) {
    // Box-Muller transform
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

static void print_header(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║           IMU FUSION DEMO - EKF-BASED SENSOR FUSION              ║\n");
    printf("╠══════════════════════════════════════════════════════════════════╣\n");
    printf("║  15-State Extended Kalman Filter                                ║\n");
    printf("║  • Position (N, E, D)  • Velocity (N, E, D)                      ║\n");
    printf("║  • Quaternion (w,x,y,z)  • Gyro bias (3)  • Accel bias (2)       ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");
}

static void print_section(const char* title) {
    printf("\n┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│ %-66s │\n", title);
    printf("└──────────────────────────────────────────────────────────────────┘\n");
}

// =============================================================================
// True Flight Trajectory (Ground Truth)
// =============================================================================

typedef struct {
    float t;
    eif_vec3_t pos;      // NED position (m)
    eif_vec3_t vel;      // NED velocity (m/s)
    eif_quat_t quat;     // Body-to-NED quaternion
    eif_vec3_t accel;    // True acceleration in body frame
    eif_vec3_t omega;    // True angular velocity in body frame
} flight_state_t;

// Generate a circular flight trajectory
static void generate_trajectory(float t, flight_state_t* state) {
    state->t = t;
    
    // Phase 1: Takeoff (0-5s)
    // Phase 2: Circular flight (5-25s)
    // Phase 3: Landing (25-30s)
    
    float radius = 20.0f;
    float altitude = 10.0f;
    float omega_circ = 2.0f * PI / 20.0f;  // 20 second circle
    
    if (t < 5.0f) {
        // Takeoff - straight up
        float frac = t / 5.0f;
        state->pos.x = 0.0f;
        state->pos.y = 0.0f;
        state->pos.z = -altitude * frac * frac;  // NED: down is positive
        
        state->vel.x = 0.0f;
        state->vel.y = 0.0f;
        state->vel.z = -2.0f * altitude * frac / 5.0f;
        
        state->omega.x = 0.0f;
        state->omega.y = 0.0f;
        state->omega.z = 0.0f;
        
    } else if (t < 25.0f) {
        // Circular flight
        float circle_t = t - 5.0f;
        float angle = omega_circ * circle_t;
        
        state->pos.x = radius * sinf(angle);
        state->pos.y = radius * (1.0f - cosf(angle));
        state->pos.z = -altitude;
        
        state->vel.x = radius * omega_circ * cosf(angle);
        state->vel.y = radius * omega_circ * sinf(angle);
        state->vel.z = 0.0f;
        
        // Yaw to match velocity direction
        state->omega.x = 0.0f;
        state->omega.y = 0.0f;
        state->omega.z = omega_circ;
        
    } else {
        // Landing
        float land_t = t - 25.0f;
        float frac = land_t / 5.0f;
        
        float final_x = radius * sinf(omega_circ * 20.0f);
        float final_y = radius * (1.0f - cosf(omega_circ * 20.0f));
        
        state->pos.x = final_x;
        state->pos.y = final_y;
        state->pos.z = -altitude * (1.0f - frac * frac);
        
        state->vel.x = 0.0f;
        state->vel.y = 0.0f;
        state->vel.z = 2.0f * altitude * frac / 5.0f;
        
        state->omega.x = 0.0f;
        state->omega.y = 0.0f;
        state->omega.z = 0.0f;
    }
    
    // Quaternion from yaw
    float yaw = atan2f(state->vel.y, state->vel.x);
    if (fabsf(state->vel.x) < 0.01f && fabsf(state->vel.y) < 0.01f) {
        yaw = 0.0f;
    }
    state->quat.w = cosf(yaw / 2.0f);
    state->quat.x = 0.0f;
    state->quat.y = 0.0f;
    state->quat.z = sinf(yaw / 2.0f);
    
    // True acceleration in body frame (simplified)
    state->accel.x = 0.0f;
    state->accel.y = 0.0f;
    state->accel.z = 9.81f;  // Gravity in body frame (level flight)
}

// =============================================================================
// Sensor Simulation
// =============================================================================

static void simulate_imu(const flight_state_t* true_state, float accel[3], float gyro[3]) {
    // Add noise to accelerometer
    accel[0] = true_state->accel.x + random_gaussian() * ACCEL_NOISE_STD;
    accel[1] = true_state->accel.y + random_gaussian() * ACCEL_NOISE_STD;
    accel[2] = true_state->accel.z + random_gaussian() * ACCEL_NOISE_STD;
    
    // Add noise to gyroscope
    gyro[0] = true_state->omega.x + random_gaussian() * GYRO_NOISE_STD;
    gyro[1] = true_state->omega.y + random_gaussian() * GYRO_NOISE_STD;
    gyro[2] = true_state->omega.z + random_gaussian() * GYRO_NOISE_STD;
}

static void simulate_gps(const flight_state_t* true_state, 
                         double* lat, double* lon, float* alt, float vel_ned[3]) {
    // Convert NED to GPS (simple flat-earth approximation)
    double ref_lat = 41.015137;  // Istanbul
    double ref_lon = 28.979530;
    
    double dlat = true_state->pos.x / 111000.0;  // meters to degrees
    double dlon = true_state->pos.y / (111000.0 * cos(ref_lat * DEG2RAD));
    
    *lat = ref_lat + dlat + random_gaussian() * (GPS_POS_NOISE_STD / 111000.0);
    *lon = ref_lon + dlon + random_gaussian() * (GPS_POS_NOISE_STD / 111000.0);
    *alt = -true_state->pos.z + random_gaussian() * GPS_POS_NOISE_STD;
    
    vel_ned[0] = true_state->vel.x + random_gaussian() * GPS_VEL_NOISE_STD;
    vel_ned[1] = true_state->vel.y + random_gaussian() * GPS_VEL_NOISE_STD;
    vel_ned[2] = true_state->vel.z + random_gaussian() * GPS_VEL_NOISE_STD;
}

static float simulate_baro(const flight_state_t* true_state) {
    return -true_state->pos.z + random_gaussian() * BARO_NOISE_STD;
}

// =============================================================================
// Visualization
// =============================================================================

static void print_trajectory_visualization(float true_n, float true_e, 
                                            float est_n, float est_e) {
    const int WIDTH = 40;
    const int HEIGHT = 15;
    char grid[HEIGHT][WIDTH + 1];
    
    // Clear grid
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            grid[i][j] = ' ';
        }
        grid[i][WIDTH] = '\0';
    }
    
    // Scale to grid
    float scale = 50.0f;
    int true_col = (int)((true_e / scale + 0.5f) * WIDTH);
    int true_row = (int)((0.5f - true_n / scale) * HEIGHT);
    int est_col = (int)((est_e / scale + 0.5f) * WIDTH);
    int est_row = (int)((0.5f - est_n / scale) * HEIGHT);
    
    true_col = (true_col < 0) ? 0 : (true_col >= WIDTH) ? WIDTH-1 : true_col;
    true_row = (true_row < 0) ? 0 : (true_row >= HEIGHT) ? HEIGHT-1 : true_row;
    est_col = (est_col < 0) ? 0 : (est_col >= WIDTH) ? WIDTH-1 : est_col;
    est_row = (est_row < 0) ? 0 : (est_row >= HEIGHT) ? HEIGHT-1 : est_row;
    
    grid[true_row][true_col] = 'T';  // True position
    if (est_row != true_row || est_col != true_col) {
        grid[est_row][est_col] = 'E';  // Estimated position
    }
    
    // Print grid
    printf("    ╔");
    for (int j = 0; j < WIDTH; j++) printf("═");
    printf("╗\n");
    
    for (int i = 0; i < HEIGHT; i++) {
        printf("    ║%s║\n", grid[i]);
    }
    
    printf("    ╚");
    for (int j = 0; j < WIDTH; j++) printf("═");
    printf("╝\n");
    
    printf("    T = True Position, E = EKF Estimate\n");
}

static void print_bar(const char* label, float value, float max_val, int width) {
    int filled = (int)(fabsf(value) / max_val * width);
    if (filled > width) filled = width;
    
    printf("    %-8s [", label);
    if (value >= 0) {
        for (int i = 0; i < width/2; i++) printf(" ");
        for (int i = 0; i < filled && i < width/2; i++) printf("█");
        for (int i = filled; i < width/2; i++) printf(" ");
    } else {
        for (int i = 0; i < width/2 - filled; i++) printf(" ");
        for (int i = 0; i < filled && i < width/2; i++) printf("█");
        for (int i = 0; i < width/2; i++) printf(" ");
    }
    printf("] %+7.2f\n", value);
}

// =============================================================================
// JSON Output (for visualization tools)
// =============================================================================

static void print_json_state(float t, const flight_state_t* true_state, 
                              const eif_pose_t* pose, const float* accel, const float* gyro) {
    printf("{\"t\": %.3f, ", t);
    printf("\"type\": \"imu_fusion\", ");
    
    // Sensor data
    printf("\"sensors\": {");
    printf("\"ax\": %.4f, \"ay\": %.4f, \"az\": %.4f, ", accel[0], accel[1], accel[2]);
    printf("\"gx\": %.4f, \"gy\": %.4f, \"gz\": %.4f", gyro[0], gyro[1], gyro[2]);
    printf("}, ");
    
    // True state
    printf("\"true\": {");
    printf("\"x\": %.3f, \"y\": %.3f, \"z\": %.3f, ", 
           true_state->pos.x, true_state->pos.y, true_state->pos.z);
    printf("\"vx\": %.3f, \"vy\": %.3f, \"vz\": %.3f",
           true_state->vel.x, true_state->vel.y, true_state->vel.z);
    printf("}, ");
    
    // EKF estimate
    printf("\"estimate\": {");
    printf("\"x\": %.3f, \"y\": %.3f, \"z\": %.3f, ",
           pose->position.x, pose->position.y, pose->position.z);
    printf("\"vx\": %.3f, \"vy\": %.3f, \"vz\": %.3f, ",
           pose->velocity.x, pose->velocity.y, pose->velocity.z);
    printf("\"roll\": %.2f, \"pitch\": %.2f, \"yaw\": %.2f",
           pose->euler.x * RAD2DEG, pose->euler.y * RAD2DEG, pose->euler.z * RAD2DEG);
    printf("}, ");
    
    // Error metrics
    float pos_err = sqrtf(
        (pose->position.x - true_state->pos.x) * (pose->position.x - true_state->pos.x) +
        (pose->position.y - true_state->pos.y) * (pose->position.y - true_state->pos.y) +
        (pose->position.z - true_state->pos.z) * (pose->position.z - true_state->pos.z));
    
    printf("\"error\": {\"pos\": %.4f}", pos_err);
    printf("}\n");
    fflush(stdout);
}

// =============================================================================
// Main Demo
// =============================================================================

int main(int argc, char** argv) {
    // Parse command line
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_output = true;
        }
    }
    print_header();
    
    // Initialize memory pool
    static uint8_t memory_buffer[64 * 1024];
    eif_memory_pool_t pool;
    eif_memory_pool_init(&pool, memory_buffer, sizeof(memory_buffer));
    
    // Initialize IMU fusion
    eif_imu_t imu;
    eif_imu_config_t config;
    eif_imu_default_config(&config);
    
    // Set reference location (Istanbul)
    config.ref_latitude = 41.015137;
    config.ref_longitude = 28.979530;
    config.ref_altitude = 0.0f;
    
    eif_status_t status = eif_imu_init(&imu, &config, &pool);
    if (status != EIF_STATUS_OK) {
        printf("ERROR: Failed to initialize IMU fusion: %d\n", status);
        return 1;
    }
    
    print_section("CONFIGURATION");
    printf("    IMU Rate:     %.0f Hz\n", IMU_RATE_HZ);
    printf("    GPS Rate:     %.0f Hz\n", GPS_RATE_HZ);
    printf("    Baro Rate:    %.0f Hz\n", BARO_RATE_HZ);
    printf("    Duration:     %.0f seconds\n", DEMO_DURATION_SEC);
    printf("    Filter:       Extended Kalman Filter (EKF)\n");
    printf("    State Dim:    15 states\n");
    
    print_section("FLIGHT SIMULATION");
    printf("    Phase 1: Takeoff (0-5s) - Ascending to 10m\n");
    printf("    Phase 2: Circle (5-25s) - Radius 20m at 10m altitude\n");
    printf("    Phase 3: Land (25-30s) - Descending to ground\n");
    
    // Simulation loop
    float dt_imu = 1.0f / IMU_RATE_HZ;
    float dt_gps = 1.0f / GPS_RATE_HZ;
    float dt_baro = 1.0f / BARO_RATE_HZ;
    
    float t = 0.0f;
    float last_gps_t = -dt_gps;
    float last_baro_t = -dt_baro;
    float next_print_t = 0.0f;
    
    // Error tracking
    float pos_rmse_sum = 0.0f;
    float vel_rmse_sum = 0.0f;
    int sample_count = 0;
    
    srand(42);  // Reproducible results
    
    print_section("RUNNING SIMULATION");
    
    while (t <= DEMO_DURATION_SEC) {
        flight_state_t true_state;
        generate_trajectory(t, &true_state);
        
        // IMU update (every step)
        float accel[3], gyro[3];
        simulate_imu(&true_state, accel, gyro);
        eif_imu_update_sensors(&imu, accel, gyro, dt_imu);
        
        // GPS update
        if (t - last_gps_t >= dt_gps) {
            double lat, lon;
            float alt, vel_ned[3];
            simulate_gps(&true_state, &lat, &lon, &alt, vel_ned);
            eif_imu_update_gps(&imu, lat, lon, alt, vel_ned);
            last_gps_t = t;
        }
        
        // Barometer update
        if (t - last_baro_t >= dt_baro) {
            float baro_alt = simulate_baro(&true_state);
            eif_imu_update_baro(&imu, baro_alt);
            last_baro_t = t;
        }
        
        // Get estimates
        eif_pose_t pose;
        eif_imu_get_pose(&imu, &pose);
        
        // Compute errors
        float pos_err_n = pose.position.x - true_state.pos.x;
        float pos_err_e = pose.position.y - true_state.pos.y;
        float pos_err_d = pose.position.z - true_state.pos.z;
        float pos_err = sqrtf(pos_err_n*pos_err_n + pos_err_e*pos_err_e + pos_err_d*pos_err_d);
        
        float vel_err_n = pose.velocity.x - true_state.vel.x;
        float vel_err_e = pose.velocity.y - true_state.vel.y;
        float vel_err_d = pose.velocity.z - true_state.vel.z;
        float vel_err = sqrtf(vel_err_n*vel_err_n + vel_err_e*vel_err_e + vel_err_d*vel_err_d);
        
        pos_rmse_sum += pos_err * pos_err;
        vel_rmse_sum += vel_err * vel_err;
        sample_count++;
        
        // JSON output mode (every 10 samples = 10Hz)
        if (json_output && sample_count % 10 == 0) {
            print_json_state(t, &true_state, &pose, accel, gyro);
            continue;  // Skip normal output in JSON mode
        }
        
        // Print progress every 5 seconds (normal mode)
        if (t >= next_print_t) {
            printf("\n    ═══ T = %.1f sec ═══\n", t);
            printf("\n    TRUE STATE:\n");
            printf("      Position: N=%+7.2fm  E=%+7.2fm  D=%+7.2fm\n",
                   true_state.pos.x, true_state.pos.y, true_state.pos.z);
            printf("      Velocity: N=%+6.2fm/s E=%+6.2fm/s D=%+6.2fm/s\n",
                   true_state.vel.x, true_state.vel.y, true_state.vel.z);
            
            printf("\n    EKF ESTIMATE:\n");
            printf("      Position: N=%+7.2fm  E=%+7.2fm  D=%+7.2fm\n",
                   pose.position.x, pose.position.y, pose.position.z);
            printf("      Velocity: N=%+6.2fm/s E=%+6.2fm/s D=%+6.2fm/s\n",
                   pose.velocity.x, pose.velocity.y, pose.velocity.z);
            printf("      Attitude: R=%+6.1f°  P=%+6.1f°  Y=%+6.1f°\n",
                   pose.euler.x * RAD2DEG, pose.euler.y * RAD2DEG, pose.euler.z * RAD2DEG);
            
            printf("\n    ERROR:\n");
            printf("      Position: %.3f m\n", pos_err);
            printf("      Velocity: %.3f m/s\n", vel_err);
            
            // 2D trajectory visualization
            printf("\n    TRAJECTORY (Top View - N/E plane):\n");
            print_trajectory_visualization(true_state.pos.x, true_state.pos.y,
                                            pose.position.x, pose.position.y);
            
            next_print_t += 5.0f;
        }
        
        t += dt_imu;
    }
    
    // Final statistics
    float pos_rmse = sqrtf(pos_rmse_sum / sample_count);
    float vel_rmse = sqrtf(vel_rmse_sum / sample_count);
    
    print_section("FINAL STATISTICS");
    printf("\n    RMSE Position: %.3f m\n", pos_rmse);
    printf("    RMSE Velocity: %.3f m/s\n", vel_rmse);
    
    // Get final biases
    eif_vec3_t gyro_bias;
    float accel_bias_z;
    eif_imu_get_biases(&imu, &gyro_bias, &accel_bias_z);
    
    printf("\n    ESTIMATED BIASES:\n");
    printf("      Gyro:  X=%+.5f rad/s  Y=%+.5f rad/s  Z=%+.5f rad/s\n",
           gyro_bias.x, gyro_bias.y, gyro_bias.z);
    printf("      Accel Z: %+.4f m/s²\n", accel_bias_z);
    
    // Visual summary
    printf("\n    ACCURACY ASSESSMENT:\n");
    print_bar("Pos Err", pos_rmse, 5.0f, 30);
    print_bar("Vel Err", vel_rmse, 1.0f, 30);
    
    if (pos_rmse < 3.0f && vel_rmse < 0.5f) {
        printf("\n    ✅ EKF fusion performing well!\n");
    } else if (pos_rmse < 5.0f && vel_rmse < 1.0f) {
        printf("\n    ⚠️  EKF fusion acceptable, could tune parameters.\n");
    } else {
        printf("\n    ❌ EKF needs parameter tuning.\n");
    }
    
    print_section("DEMO COMPLETE");
    printf("    EKF-based IMU fusion demonstrated successfully!\n\n");
    
    return 0;
}
