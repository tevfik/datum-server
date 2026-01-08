/**
 * @file main.c
 * @brief Smart Sensor Hub Demo
 * 
 * Multi-sensor fusion and anomaly detection for IoT applications.
 * 
 * Features:
 * - Multiple sensor inputs (temp, humidity, pressure, light)
 * - Running statistics (mean, variance)
 * - Anomaly detection per sensor
 * - MQTT-ready output format
 * 
 * Target: ESP32-mini or any ESP32 variant
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "eif_memory.h"

#define NUM_SENSORS 4
#define WINDOW_SIZE 50
#define ANOMALY_THRESHOLD 3.0f

static uint8_t pool_buffer[32 * 1024];
static eif_memory_pool_t pool;

typedef struct {
    const char* name;
    const char* unit;
    float min_val;
    float max_val;
    float normal_mean;
    float normal_std;
} sensor_config_t;

typedef struct {
    float history[WINDOW_SIZE];
    int idx;
    int count;
    float sum;
    float sum_sq;
} running_stats_t;

typedef struct {
    float value;
    float mean;
    float std;
    float z_score;
    int is_anomaly;
} sensor_reading_t;

static const sensor_config_t sensor_configs[NUM_SENSORS] = {
    {"Temperature", "°C",    -10.0f, 50.0f,  22.0f, 2.0f},
    {"Humidity",    "%",       0.0f, 100.0f, 45.0f, 5.0f},
    {"Pressure",    "hPa",   950.0f, 1050.0f, 1013.0f, 5.0f},
    {"Light",       "lux",    0.0f, 10000.0f, 500.0f, 100.0f}
};

static running_stats_t stats[NUM_SENSORS];

static void stats_init(running_stats_t* s) {
    memset(s, 0, sizeof(running_stats_t));
}

static void stats_update(running_stats_t* s, float value) {
    if (s->count >= WINDOW_SIZE) {
        // Remove oldest value
        float old = s->history[s->idx];
        s->sum -= old;
        s->sum_sq -= old * old;
    } else {
        s->count++;
    }
    
    s->history[s->idx] = value;
    s->sum += value;
    s->sum_sq += value * value;
    s->idx = (s->idx + 1) % WINDOW_SIZE;
}

static float stats_mean(const running_stats_t* s) {
    return s->count > 0 ? s->sum / s->count : 0.0f;
}

static float stats_std(const running_stats_t* s) {
    if (s->count < 2) return 0.0f;
    float mean = stats_mean(s);
    float var = s->sum_sq / s->count - mean * mean;
    return var > 0 ? sqrtf(var) : 0.0f;
}

static float simulate_sensor(int sensor_id, int time_step) {
    const sensor_config_t* cfg = &sensor_configs[sensor_id];
    float base = cfg->normal_mean;
    
    // Add daily cycle for temperature
    if (sensor_id == 0) {
        base += 5.0f * sinf(2 * M_PI * time_step / 240.0f);
    }
    
    // Random noise
    float noise = cfg->normal_std * ((float)rand() / RAND_MAX - 0.5f) * 2;
    
    // Occasional anomaly
    if (rand() % 50 == 0) {
        noise += cfg->normal_std * 5.0f * ((rand() % 2) ? 1 : -1);
    }
    
    float value = base + noise;
    
    // Clamp to valid range
    if (value < cfg->min_val) value = cfg->min_val;
    if (value > cfg->max_val) value = cfg->max_val;
    
    return value;
}

static void print_json_output(const sensor_reading_t readings[NUM_SENSORS], int timestamp) {
    printf("{\"timestamp\": %d, \"sensors\": [", timestamp);
    for (int i = 0; i < NUM_SENSORS; i++) {
        printf("{\"name\": \"%s\", \"value\": %.1f, \"unit\": \"%s\", \"z_score\": %.2f, \"anomaly\": %s}",
               sensor_configs[i].name, readings[i].value, sensor_configs[i].unit,
               readings[i].z_score, readings[i].is_anomaly ? "true" : "false");
        if (i < NUM_SENSORS - 1) printf(", ");
    }
    printf("]}\n");
}

int main(void) {
    srand(time(NULL));
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║     Smart Sensor Hub Demo (IoT Ready)                  ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n\n");
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    // Initialize statistics
    for (int i = 0; i < NUM_SENSORS; i++) {
        stats_init(&stats[i]);
    }
    
    printf("Monitoring %d sensors: Temperature, Humidity, Pressure, Light\n", NUM_SENSORS);
    printf("Anomaly threshold: %.1f standard deviations\n\n", ANOMALY_THRESHOLD);
    
    // Table header
    printf("+-------+------------+------------+------------+------------+\n");
    printf("| Time  | Temp (C)   | Humid (%%)  | Press(hPa) | Light(lux) |\n");
    printf("+-------+------------+------------+------------+------------+\n");
    
    int anomaly_count = 0;
    
    for (int t = 0; t < 100; t++) {
        sensor_reading_t readings[NUM_SENSORS];
        int has_anomaly = 0;
        
        for (int i = 0; i < NUM_SENSORS; i++) {
            readings[i].value = simulate_sensor(i, t);
            stats_update(&stats[i], readings[i].value);
            
            readings[i].mean = stats_mean(&stats[i]);
            readings[i].std = stats_std(&stats[i]);
            
            if (readings[i].std > 0.01f) {
                readings[i].z_score = fabsf(readings[i].value - readings[i].mean) / readings[i].std;
            } else {
                readings[i].z_score = 0.0f;
            }
            
            readings[i].is_anomaly = readings[i].z_score > ANOMALY_THRESHOLD;
            if (readings[i].is_anomaly) {
                has_anomaly = 1;
                anomaly_count++;
            }
        }
        
        // Print row
        if (t < 15 || has_anomaly) {
            printf("| %5d | %6.1f %s  | %6.1f %s  | %6.1f %s  | %6.0f %s  |\n",
                   t,
                   readings[0].value, readings[0].is_anomaly ? "!" : " ",
                   readings[1].value, readings[1].is_anomaly ? "!" : " ",
                   readings[2].value, readings[2].is_anomaly ? "!" : " ",
                   readings[3].value, readings[3].is_anomaly ? "!" : " ");
        } else if (t == 15) {
            printf("|  ...  |    ...     |    ...     |    ...     |    ...     |\n");
        }
    }
    
    printf("+-------+------------+------------+------------+------------+\n");
    printf("\nTotal anomalies detected: %d\n", anomaly_count);
    printf("(!) = Anomaly detected (>%.1f σ from mean)\n\n", ANOMALY_THRESHOLD);
    
    printf("Example MQTT JSON output:\n");
    sensor_reading_t sample[NUM_SENSORS];
    for (int i = 0; i < NUM_SENSORS; i++) {
        sample[i].value = simulate_sensor(i, 50);
        sample[i].z_score = 0.5f;
        sample[i].is_anomaly = 0;
    }
    print_json_output(sample, 50);
    
    printf("\n┌─────────────────────────────────────────────────────────┐\n");
    printf("│  For ESP32 deployment:                                  │\n");
    printf("│  1. Add I2C drivers for BME280/BH1750 sensors           │\n");
    printf("│  2. Connect to WiFi and MQTT broker                     │\n");
    printf("│  3. Publish JSON to home automation (Home Assistant)    │\n");
    printf("└─────────────────────────────────────────────────────────┘\n\n");
    
    return 0;
}
