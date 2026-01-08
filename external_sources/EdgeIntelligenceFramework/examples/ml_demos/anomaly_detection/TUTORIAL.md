# Anomaly Detection Tutorial: Predictive Maintenance

## Learning Objectives

By the end of this tutorial, you will understand:
- Statistical anomaly detection (Z-score, EWMA)
- Machine learning approaches (Isolation Forest)
- Ensemble methods for robust detection
- Real-time streaming anomaly detection
- Industrial IoT applications

**Level**: Beginner to Intermediate  
**Prerequisites**: Basic statistics, understanding of mean/variance  
**Time**: 45-60 minutes

---

## 1. Introduction to Anomaly Detection

### What is Anomaly Detection?

Identifying data points that differ significantly from the norm:
- **Point anomalies**: Single unusual values
- **Contextual anomalies**: Unusual in specific context
- **Collective anomalies**: Groups of related outliers

### Industrial Applications

| Industry | Application | Sensors |
|----------|-------------|---------|
| **Manufacturing** | Machine health | Vibration, temperature |
| **Energy** | Grid monitoring | Current, voltage |
| **Transportation** | Vehicle diagnostics | Engine data |
| **Data Centers** | Server health | CPU temp, fan speed |

---

## 2. Statistical Methods

### 2.1 Z-Score

Measures how many standard deviations from mean:

```
z = (x - μ) / σ

If |z| > 3: Likely anomaly (99.7% confidence)
```

```c
// EIF implementation
eif_stat_detector_t detector;
eif_stat_detector_init(&detector, window_size, threshold, &pool);

// Update with new data
eif_stat_detector_update(&detector, value);

// Check for anomaly
if (eif_stat_detector_is_anomaly(&detector, value)) {
    trigger_alert();
}
```

### 2.2 EWMA (Exponentially Weighted Moving Average)

Gives more weight to recent data:

```
EWMA_t = α × x_t + (1 - α) × EWMA_{t-1}

where α ∈ (0, 1) is the smoothing factor
```

```c
eif_ts_detector_t ts_detector;
eif_ts_detector_init(&ts_detector, 
    0.1f,    // alpha (smoothing)
    3.0f,    // threshold (std devs)
    &pool);

float score = eif_ts_detector_update(&ts_detector, value);
if (score > 0.8f) {
    // High anomaly score
}
```

---

## 3. Machine Learning Methods

### 3.1 Isolation Forest

**Key Insight**: Anomalies are easier to isolate!

```
Normal point:  Many splits needed to isolate
Anomaly:       Few splits needed (stands out)

Anomaly Score = 2^(-avg_path_length / c(n))
```

```c
// Initialize
eif_iforest_t iforest;
eif_iforest_init(&iforest, n_features, n_trees, &pool);

// Train on normal data
eif_iforest_fit(&iforest, normal_data, n_samples);

// Score new data (higher = more anomalous)
float score = eif_iforest_score(&iforest, sample);
if (score > 0.6f) {
    // Likely anomaly
}
```

### 3.2 Multivariate Detector

Combines multiple features:

```c
eif_mv_detector_t mv_detector;
eif_mv_detector_init(&mv_detector, 
    NUM_FEATURES,  // e.g., 3
    10,            // history size
    0.6f,          // threshold
    &pool);

// Fit on normal data
eif_mv_detector_fit(&mv_detector, train_data, n_samples);

// Test
if (eif_mv_detector_is_anomaly(&mv_detector, sample)) {
    alert();
}
```

---

## 4. Ensemble Methods

### Why Ensemble?

| Method | Strength | Weakness |
|--------|----------|----------|
| Z-Score | Fast, simple | Univariate only |
| EWMA | Temporal patterns | Slow to adapt |
| Isolation Forest | Multivariate | Training required |

**Ensemble combines strengths!**

### Implementation

```c
eif_ensemble_detector_t ensemble;
eif_ensemble_init(&ensemble, NUM_FEATURES, window_size, &pool);

// Fit on normal data
eif_ensemble_fit(&ensemble, normal_data, n_samples);

// Get combined score
float score = eif_ensemble_score(&ensemble, sample);
// score ∈ [0, 1], higher = more anomalous
```

---

## 5. Code Walkthrough

### Sensor Data Structure

```c
#define NUM_FEATURES 3

// Feature indices
#define TEMP_IDX 0    // Temperature (°C)
#define VIB_IDX  1    // Vibration (g)
#define CURR_IDX 2    // Current (A)

float sample[NUM_FEATURES] = {
    62.5f,    // Temperature
    0.18f,    // Vibration
    5.8f      // Current
};
```

### Training Phase

```c
// Generate/collect normal operation data
float32_t train_data[NORMAL_SAMPLES * NUM_FEATURES];

// Collect over normal operation period
for (int i = 0; i < NORMAL_SAMPLES; i++) {
    read_sensors(&train_data[i * NUM_FEATURES]);
    delay_ms(100);
}

// Train detector
eif_ensemble_fit(&ensemble, train_data, NORMAL_SAMPLES);
```

### Detection Phase

```c
while (1) {
    float sample[NUM_FEATURES];
    read_sensors(sample);
    
    float score = eif_ensemble_score(&ensemble, sample);
    
    if (score > THRESHOLD) {
        printf("ANOMALY: score=%.2f\n", score);
        printf("  Temp: %.1f°C, Vib: %.2fg, Curr: %.1fA\n",
               sample[0], sample[1], sample[2]);
        trigger_alarm();
    }
    
    delay_ms(100);
}
```

---

## 6. Tuning Parameters

### Threshold Selection

| Threshold | False Positives | Missed Anomalies |
|-----------|----------------|------------------|
| 0.3 (low) | High | Low |
| 0.5 (medium) | Medium | Medium |
| 0.7 (high) | Low | High |

**Recommendation**: Start with 0.5, adjust based on domain.

### Window Size

| Window | Memory | Adaptability |
|--------|--------|--------------|
| 10 | Low | Fast adaptation |
| 50 | Medium | Balanced |
| 200 | High | Stable baseline |

---

## 7. Experiments

### Experiment 1: Different Anomaly Types
Inject temperature, vibration, and current anomalies separately.

### Experiment 2: Sensitivity Analysis
Vary thresholds from 0.3 to 0.9 and measure precision/recall.

### Experiment 3: Streaming Mode
Test with continuous data stream, measure latency.

---

## 8. Hardware Deployment

### ESP32 with Sensors

```c
#include "eif_anomaly.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

// ADC channels for sensors
#define TEMP_CHANNEL ADC1_CHANNEL_6  // GPIO34
#define VIB_CHANNEL  ADC1_CHANNEL_7  // GPIO35
#define CURR_CHANNEL ADC1_CHANNEL_4  // GPIO32

void sensor_task(void* arg) {
    float sample[3];
    
    while (1) {
        // Read sensors
        sample[0] = read_temperature();
        sample[1] = read_vibration();
        sample[2] = read_current();
        
        // Check for anomaly
        float score = eif_ensemble_score(&ensemble, sample);
        
        if (score > 0.5f) {
            // Send alert via MQTT
            mqtt_publish("alerts/machine1", score);
            gpio_set_level(LED_GPIO, 1);  // Alert LED
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
```

### Memory Requirements

| Component | RAM |
|-----------|-----|
| Ensemble detector | 4 KB |
| Sample buffer | 1 KB |
| Working memory | 2 KB |
| **Total** | **~7 KB** |

Compatible with ESP32-mini!

---

## 9. Summary

### Key Concepts
1. **Z-Score**: Statistical deviation detection
2. **EWMA**: Time-series smoothing
3. **Isolation Forest**: ML-based multivariate
4. **Ensemble**: Combine methods for robustness

### EIF APIs
- `eif_stat_detector_*()` - Statistical detector
- `eif_ts_detector_*()` - Time series (EWMA)
- `eif_mv_detector_*()` - Multivariate
- `eif_ensemble_*()` - Combined ensemble

### Next Steps
- Try `time_series` for forecasting
- Implement on ESP32 with real sensors
- Add MQTT for cloud alerting
