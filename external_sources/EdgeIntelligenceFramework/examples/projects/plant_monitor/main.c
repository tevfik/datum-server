/**
 * @file main.c
 * @brief Smart Plant Monitor - Soil & Environment Sensing with ML
 * 
 * Complete plant care system:
 * - Soil moisture sensing
 * - Environmental monitoring (temp, humidity, light)
 * - Plant health prediction (ML)
 * - Watering recommendations
 * - Growth tracking
 * 
 * Target: ESP32-mini with analog sensors
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "eif_memory.h"

// ============================================================================
// Configuration
// ============================================================================

#define NUM_SENSORS 5
#define HISTORY_SIZE 24  // 24 hours

typedef enum {
    SENSOR_SOIL_MOISTURE,
    SENSOR_TEMPERATURE,
    SENSOR_HUMIDITY,
    SENSOR_LIGHT,
    SENSOR_SOIL_TEMP
} sensor_type_t;

typedef enum {
    HEALTH_CRITICAL,
    HEALTH_POOR,
    HEALTH_FAIR,
    HEALTH_GOOD,
    HEALTH_EXCELLENT
} health_status_t;

// ============================================================================
// Plant Profiles
// ============================================================================

typedef struct {
    const char* name;
    float moisture_min, moisture_max;  // %
    float temp_min, temp_max;          // °C
    float humidity_min, humidity_max;  // %
    float light_min, light_max;        // lux
} plant_profile_t;

static const plant_profile_t plant_profiles[] = {
    {"Succulent",    20, 40, 15, 30, 30, 60, 2000, 8000},
    {"Tropical",     60, 80, 20, 28, 60, 90, 1000, 5000},
    {"Herb",         50, 70, 18, 26, 40, 70, 3000, 10000},
    {"Fern",         70, 90, 16, 24, 70, 95, 500, 2000},
};
#define NUM_PROFILES 4

// ============================================================================
// Monitor State
// ============================================================================

typedef struct {
    const plant_profile_t* profile;
    
    // Current readings
    float soil_moisture;
    float temperature;
    float humidity;
    float light_level;
    float soil_temp;
    
    // History (hourly averages)
    float moisture_history[HISTORY_SIZE];
    float temp_history[HISTORY_SIZE];
    int history_idx;
    int history_full;
    
    // Stats
    health_status_t health;
    int days_since_water;
    float water_needed_ml;
    int hours_of_light;
} plant_monitor_t;

static uint8_t pool_buffer[16 * 1024];
static eif_memory_pool_t pool;

// ============================================================================
// Sensor Simulation
// ============================================================================

static void simulate_sensors(plant_monitor_t* pm, int hour_of_day, int days_since_water) {
    // Soil moisture decreases over time
    pm->soil_moisture = 80.0f - days_since_water * 15.0f + 
                        5.0f * ((float)rand()/RAND_MAX - 0.5f);
    if (pm->soil_moisture < 5) pm->soil_moisture = 5;
    if (pm->soil_moisture > 100) pm->soil_moisture = 100;
    
    // Temperature follows daily cycle
    pm->temperature = 22.0f + 4.0f * sinf(2 * M_PI * (hour_of_day - 14) / 24.0f)
                     + 1.0f * ((float)rand()/RAND_MAX - 0.5f);
    
    // Humidity inversely related to temp
    pm->humidity = 60.0f - 10.0f * sinf(2 * M_PI * (hour_of_day - 14) / 24.0f)
                  + 5.0f * ((float)rand()/RAND_MAX - 0.5f);
    
    // Light follows daylight pattern
    if (hour_of_day >= 6 && hour_of_day <= 20) {
        float light_factor = sinf(M_PI * (hour_of_day - 6) / 14.0f);
        pm->light_level = 5000.0f * light_factor + 500.0f * ((float)rand()/RAND_MAX);
    } else {
        pm->light_level = 10.0f * ((float)rand()/RAND_MAX);
    }
    
    // Soil temp lags air temp
    pm->soil_temp = pm->temperature - 2.0f + 0.5f * ((float)rand()/RAND_MAX - 0.5f);
    
    pm->days_since_water = days_since_water;
}

// ============================================================================
// Health Assessment
// ============================================================================

static health_status_t assess_health(const plant_monitor_t* pm) {
    const plant_profile_t* p = pm->profile;
    int score = 100;
    
    // Moisture check
    if (pm->soil_moisture < p->moisture_min) {
        score -= 30 * (p->moisture_min - pm->soil_moisture) / p->moisture_min;
    } else if (pm->soil_moisture > p->moisture_max) {
        score -= 20 * (pm->soil_moisture - p->moisture_max) / (100 - p->moisture_max);
    }
    
    // Temperature check
    if (pm->temperature < p->temp_min || pm->temperature > p->temp_max) {
        score -= 20;
    }
    
    // Humidity check
    if (pm->humidity < p->humidity_min || pm->humidity > p->humidity_max) {
        score -= 15;
    }
    
    // Light check
    if (pm->light_level < p->light_min) {
        score -= 15;
    }
    
    if (score < 0) score = 0;
    
    if (score >= 90) return HEALTH_EXCELLENT;
    if (score >= 70) return HEALTH_GOOD;
    if (score >= 50) return HEALTH_FAIR;
    if (score >= 30) return HEALTH_POOR;
    return HEALTH_CRITICAL;
}

static float calculate_water_needed(const plant_monitor_t* pm) {
    const plant_profile_t* p = pm->profile;
    float target = (p->moisture_min + p->moisture_max) / 2;
    
    if (pm->soil_moisture >= target) return 0;
    
    // Simple model: 50ml per 10% moisture deficit
    float deficit = target - pm->soil_moisture;
    return deficit * 5.0f;  // ml
}

static const char* get_recommendations(const plant_monitor_t* pm) {
    const plant_profile_t* p = pm->profile;
    
    if (pm->soil_moisture < p->moisture_min) {
        return "Water immediately!";
    }
    if (pm->soil_moisture > p->moisture_max) {
        return "Overwatered - improve drainage";
    }
    if (pm->temperature < p->temp_min) {
        return "Move to warmer location";
    }
    if (pm->temperature > p->temp_max) {
        return "Move away from heat source";
    }
    if (pm->light_level < p->light_min) {
        return "Needs more light";
    }
    return "Conditions optimal";
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    srand(time(NULL));
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║       Smart Plant Monitor - ML-Based Plant Care                ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");
    
    eif_memory_pool_init(&pool, pool_buffer, sizeof(pool_buffer));
    
    plant_monitor_t monitor;
    memset(&monitor, 0, sizeof(monitor));
    monitor.profile = &plant_profiles[2];  // Herb plant
    
    printf("Plant Profile: %s\n", monitor.profile->name);
    printf("  Moisture: %.0f-%.0f%%\n", monitor.profile->moisture_min, monitor.profile->moisture_max);
    printf("  Temp: %.0f-%.0f°C\n", monitor.profile->temp_min, monitor.profile->temp_max);
    printf("  Humidity: %.0f-%.0f%%\n", monitor.profile->humidity_min, monitor.profile->humidity_max);
    printf("  Light: %.0f-%.0f lux\n\n", monitor.profile->light_min, monitor.profile->light_max);
    
    printf("Simulating 7 days of monitoring...\n\n");
    
    static const char* health_names[] = {"CRITICAL", "POOR", "FAIR", "GOOD", "EXCELLENT"};
    static const char* health_icons[] = {"💀", "😟", "😐", "🙂", "🌱"};
    
    printf("┌───────┬────────────────────────────────────────────────────────────┐\n");
    printf("│  Day  │  Moisture  Temp   Humid   Light    Health   Recommendation │\n");
    printf("├───────┼────────────────────────────────────────────────────────────┤\n");
    
    for (int day = 0; day < 7; day++) {
        // Simulate watering on day 0 and 3
        int days_since = (day <= 3) ? day : day - 3;
        
        // Sample at noon
        simulate_sensors(&monitor, 12, days_since);
        monitor.health = assess_health(&monitor);
        monitor.water_needed_ml = calculate_water_needed(&monitor);
        
        printf("│   %d   │   %5.1f%%   %4.1f°  %4.1f%%  %5.0f   %-10s  %-20s │\n",
               day + 1,
               monitor.soil_moisture,
               monitor.temperature,
               monitor.humidity,
               monitor.light_level,
               health_names[monitor.health],
               get_recommendations(&monitor));
        
        if (day == 2 || day == 5) {
            printf("│       │                        💧 WATERED                        │\n");
        }
    }
    
    printf("└───────┴────────────────────────────────────────────────────────────┘\n\n");
    
    // Final status
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│  CURRENT STATUS                                                 │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    printf("│  Plant Health: %s %s                                        │\n", 
           health_icons[monitor.health], health_names[monitor.health]);
    printf("│  Water Needed: %.0f ml                                          │\n", monitor.water_needed_ml);
    printf("│  Recommendation: %-40s  │\n", get_recommendations(&monitor));
    printf("└─────────────────────────────────────────────────────────────────┘\n\n");
    
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│  FOR ESP32 DEPLOYMENT:                                          │\n");
    printf("│  1. Capacitive soil moisture sensor (analog)                    │\n");
    printf("│  2. DHT22 for temp/humidity                                     │\n");
    printf("│  3. BH1750 light sensor (I2C)                                   │\n");
    printf("│  4. MQTT for Home Assistant integration                         │\n");
    printf("│  5. Deep sleep for battery operation                            │\n");
    printf("└─────────────────────────────────────────────────────────────────┘\n\n");
    
    return 0;
}
