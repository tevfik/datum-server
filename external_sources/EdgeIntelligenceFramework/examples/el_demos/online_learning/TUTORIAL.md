# Online Learning Tutorial: Adapting to Streaming Data

## Learning Objectives

- Stochastic Gradient Descent (SGD)
- Concept drift handling
- Streaming classification
- Adaptive learning rate

**Level**: Beginner to Intermediate  
**Time**: 30 minutes

---

## 1. Online vs Batch Learning

| Batch Learning | Online Learning |
|----------------|-----------------|
| Train on full dataset | Train sample-by-sample |
| Fixed model | Continuously adapts |
| Re-train for updates | Instant updates |

---

## 2. Streaming SGD

```c
// For each new sample:
prediction = dot(weights, features)
error = label - prediction
weights += learning_rate * error * features
```

### EIF Implementation

```c
eif_online_learner_t learner;
eif_online_learner_init(&learner, n_features, learning_rate, &pool);

// Streaming updates
while (data_available()) {
    float features[N_FEATURES];
    int label;
    read_sample(&features, &label);
    
    // Predict and update
    int pred = eif_online_learner_predict(&learner, features);
    eif_online_learner_update(&learner, features, label);
    
    // Track accuracy
    if (pred == label) correct++;
}
```

---

## 3. Concept Drift

### Problem
Data distribution changes over time:
- Seasonal patterns
- User behavior changes
- Sensor degradation

### Detection

```c
eif_drift_detector_t drift;
eif_drift_detector_init(&drift, window_size, threshold);

for each prediction, label:
    eif_drift_detector_update(&drift, prediction == label);
    
    if (eif_drift_detector_detected(&drift)) {
        printf("Drift detected! Resetting model...\n");
        eif_online_learner_reset(&learner);
    }
```

---

## 4. ESP32 Sensor Adaptation

```c
// Temperature sensor that adapts to environment changes
void adaptive_sensor_task(void* arg) {
    eif_online_learner_t learner;
    eif_online_learner_init(&learner, 3, 0.01f, &pool);
    // Features: hour, humidity, previous_temp
    
    while (1) {
        float features[3] = {
            get_hour() / 24.0f,
            get_humidity() / 100.0f,
            prev_temp / 50.0f
        };
        
        float predicted = eif_online_learner_predict_regression(&learner, features);
        float actual = read_temperature();
        
        // Update model
        eif_online_learner_update_regression(&learner, features, actual);
        
        prev_temp = actual;
        vTaskDelay(60000 / portTICK_PERIOD_MS);  // Every minute
    }
}
```

---

## Summary

### Key APIs
- `eif_online_learner_init()` - Initialize
- `eif_online_learner_update()` - Train on sample
- `eif_drift_detector_*()` - Detect distribution shift

### Memory: O(n_features) - Very lightweight!
