# Matrix Profile Tutorial: Time Series Pattern Discovery

## Learning Objectives

- Understand Matrix Profile algorithm
- Detect repeating patterns (motifs)
- Find anomalies (discords)
- Apply to sensor data analysis

**Level**: Intermediate  
**Time**: 35 minutes

---

## 1. What is Matrix Profile?

Matrix Profile is a data structure that stores the distance between each subsequence and its nearest neighbor:

```
Time Series:    ▁▂▄▆█▆▄▂▁▁▂▄▆█▆▄▂▁▁▂▄▆█▆▄▂▁
                 └───────┘ └───────┘ └───────┘
                   Pattern repeats 3 times!

Matrix Profile: ▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁█▁▁▁▁▁▁▁▁
                                   ↑
                                 Anomaly!
```

### Key Concepts

| Term | Meaning |
|------|---------|
| **Subsequence** | Window of length m |
| **Distance Profile** | Distances from one subsequence to all others |
| **Matrix Profile** | Min of each distance profile |
| **Motif** | Low MP value = repeating pattern |
| **Discord** | High MP value = anomaly |

---

## 2. Algorithm (STAMP)

```
For each subsequence i:
    For each subsequence j (j ≠ i):
        Compute z-normalized Euclidean distance
    MP[i] = min(distances)
    MP_index[i] = argmin(distances)
```

### Optimizations

- **STOMP**: Reuse previous distance calculations
- **SCRIMP++**: Anytime algorithm, early results
- **GPU-STOMP**: Parallel on GPU

---

## 3. EIF Implementation

```c
eif_matrix_profile_t mp;
eif_matrix_profile_init(&mp, 
    time_series, 
    series_length,
    subsequence_length,  // typically 10-100
    &pool
);

// Compute
eif_matrix_profile_compute(&mp);

// Find top 3 motifs
int motif_indices[3];
eif_matrix_profile_find_motifs(&mp, motif_indices, 3);

// Find top 3 anomalies
int discord_indices[3];
eif_matrix_profile_find_discords(&mp, discord_indices, 3);
```

---

## 4. Applications

### 4.1 Industrial Vibration

```c
// Detect machine cycle anomalies
float vibration[10000];  // 10 seconds at 1kHz
int subseq_len = 100;    // One cycle = 100 samples

eif_matrix_profile_compute(&mp);
int anomaly = eif_matrix_profile_find_discords(&mp, discords, 1)[0];

if (mp.values[anomaly] > THRESHOLD) {
    alert("Abnormal machine cycle detected!");
}
```

### 4.2 ECG Analysis

```c
// Find heartbeat anomalies
float ecg[5000];
int subseq_len = 200;  // One heartbeat

eif_matrix_profile_find_discords(&mp, anomalies, 5);
// Check for arrhythmia
```

---

## 5. ESP32 Streaming

```c
void mp_stream_task(void* arg) {
    float buffer[1000];
    int idx = 0;
    
    while (1) {
        buffer[idx % 1000] = read_sensor();
        idx++;
        
        // Compute incremental MP every 100 samples
        if (idx % 100 == 0 && idx >= 1000) {
            eif_matrix_profile_update_streaming(&mp, buffer, 1000);
            
            if (mp.max_discord > THRESHOLD) {
                mqtt_publish("alert/anomaly", mp.max_discord);
            }
        }
        
        vTaskDelay(1 / portTICK_PERIOD_MS);  // 1kHz
    }
}
```

---

## Summary

### Key APIs
- `eif_matrix_profile_init()` - Initialize
- `eif_matrix_profile_compute()` - Full computation
- `eif_matrix_profile_find_motifs()` - Find patterns
- `eif_matrix_profile_find_discords()` - Find anomalies

### Complexity
- Time: O(n²) basic, O(n log n) with STOMP
- Memory: O(n) for MP + O(m) for FFT
