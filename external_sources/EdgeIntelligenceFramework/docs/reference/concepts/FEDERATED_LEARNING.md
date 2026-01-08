# Federated Learning on Edge Devices

A comprehensive guide to privacy-preserving collaborative learning with EIF.

---

## Introduction

Federated Learning (FL) enables multiple edge devices to collaboratively train ML models **without sharing raw data**. Each device:

1. Trains locally on its private data
2. Sends only model updates (gradients/weights) to aggregator
3. Receives improved global model

```
┌─────────┐     ┌─────────┐     ┌─────────┐
│ Device1 │     │ Device2 │     │ Device3 │
│ 📱 Data │     │ 📱 Data │     │ 📱 Data │
└────┬────┘     └────┬────┘     └────┬────┘
     │               │               │
     │    weights    │    weights    │
     ▼               ▼               ▼
   ╔═══════════════════════════════════╗
   ║         Aggregation Server         ║
   ║    FedAvg: Σ(n_k/N) × w_k         ║
   ╚═══════════════════════════════════╝
     │               │               │
     │  global model │               │
     ▼               ▼               ▼
```

---

## Quick Start

### Using eif_federated.h

```c
#include "eif_federated.h"

// Initialize context
int16_t global_weights[64];
eif_fl_context_t ctx;
eif_fl_init(&ctx, global_weights, 64);

// Initialize clients
eif_fl_client_t clients[5];
int16_t client_weights[5][64];

for (int c = 0; c < 5; c++) {
    eif_fl_client_init(&clients[c], client_weights[c], 64, c);
    clients[c].sample_count = 100;  // Local data count
}

// Training loop
for (int round = 0; round < 10; round++) {
    // 1. Distribute global weights
    for (int c = 0; c < 5; c++) {
        memcpy(client_weights[c], global_weights, 64 * sizeof(int16_t));
    }
    
    // 2. Local training (your training code)
    for (int c = 0; c < 5; c++) {
        train_locally(client_weights[c], local_data[c]);
    }
    
    // 3. Aggregate with FedAvg
    eif_fl_aggregate(&ctx, clients, 5);
}
```

---

## Core Concepts

### 1. FedAvg Algorithm

The standard Federated Averaging algorithm:

```
w_global = Σ (n_k / N) × w_k

where:
  n_k = samples on client k
  N   = total samples across all clients
  w_k = weights from client k
```

**Implementation:**

```c
// Automatic weighted averaging
eif_fl_aggregate(&ctx, clients, num_clients);

// Or simple average (equal weights)
eif_fl_average(output, clients, num_clients, weight_count);
```

### 2. Gradient Compression

Reduce communication cost by sending only significant gradients:

```c
// Threshold-based sparsification
eif_fl_compress_threshold(gradients, count, threshold);

// Count non-zero gradients
int nnz = eif_fl_compress_count(gradients, count, threshold);
printf("Compression: %d -> %d (%.1f%%)\n", count, nnz, 100.0*nnz/count);
```

### 3. Differential Privacy

Add calibrated noise to protect individual data points:

```c
// Clip gradients for bounded sensitivity
eif_fl_clip_gradients(gradients, count, max_norm);

// Add Gaussian noise
eif_fl_add_noise(gradients, count, noise_scale);
```

### 4. Quantization for Communication

Reduce bandwidth with 8-bit quantization:

```c
int8_t quantized[64];
int16_t scale;

// Quantize for transmission
eif_fl_quantize_8bit(gradients, quantized, 64, &scale);

// Dequantize on receiver
eif_fl_dequantize_8bit(quantized, gradients, 64, scale);
```

---

## Privacy Guarantees

### What Data Stays Local

| Stays on Device | Sent to Server |
|-----------------|----------------|
| Raw sensor data | Model weights |
| User images/audio | Gradients (optionally compressed) |
| Personal information | Aggregated updates only |

### Privacy Enhancements

1. **Differential Privacy**: Add noise calibrated to sensitivity
2. **Secure Aggregation**: Server only sees aggregate
3. **Gradient Clipping**: Bound influence of any single sample

---

## Memory Requirements

Typical memory usage per client:

| Component | Size (bytes) |
|-----------|--------------|
| Local weights | `weight_count × 2` |
| Gradients | `weight_count × 2` |
| Aggregation buffer | `weight_count × 2` |
| **Total** | `weight_count × 6` |

Example: 1000-weight model = ~6KB per client

---

## Example: Sensor Fusion

5 factory sensors collaboratively train anomaly detector:

```c
#define NUM_CLIENTS 5
#define NUM_WEIGHTS 64

// Each factory has different sensor characteristics
// but they share the same model structure

for (int round = 0; round < FL_ROUNDS; round++) {
    // Local training
    for (int c = 0; c < NUM_CLIENTS; c++) {
        train_on_local_sensor_data(&clients[c], factory_data[c]);
    }
    
    // Privacy protection
    for (int c = 0; c < NUM_CLIENTS; c++) {
        eif_fl_clip_gradients(clients[c].weights, NUM_WEIGHTS, 1000);
        eif_fl_add_noise(clients[c].weights, NUM_WEIGHTS, 50);
    }
    
    // Aggregate
    eif_fl_aggregate(&ctx, clients, NUM_CLIENTS);
}
```

---

## Best Practices

### 1. Handle Non-IID Data

Edge devices often have different data distributions:

```c
// Weight by sample count (built into FedAvg)
clients[0].sample_count = 100;  // High-traffic device
clients[1].sample_count = 20;   // Low-traffic device
```

### 2. Use Checkpointing

Save model state for recovery:

```c
eif_fl_checkpoint_t ckpt;
int16_t ckpt_buffer[NUM_WEIGHTS];
ckpt.weights = ckpt_buffer;

// Save
eif_fl_checkpoint_save(&ckpt, global_weights, NUM_WEIGHTS, round);

// Restore after failure
eif_fl_checkpoint_restore(&ckpt, global_weights, NUM_WEIGHTS);
```

### 3. Monitor Convergence

Track weight changes to detect issues:

```c
eif_fl_aggregate(&ctx, clients, NUM_CLIENTS);
printf("Avg delta: %.4f, Max delta: %.4f\n",
       ctx.weight_delta_sum / ctx.weight_count / 32767.0f,
       ctx.max_delta / 32767.0f);
```

---

## API Reference

### Types

| Type | Description |
|------|-------------|
| `eif_fl_context_t` | Aggregation context |
| `eif_fl_client_t` | Client contribution |
| `eif_fl_checkpoint_t` | Model checkpoint |

### Functions

| Function | Description |
|----------|-------------|
| `eif_fl_init()` | Initialize FL context |
| `eif_fl_aggregate()` | FedAvg aggregation |
| `eif_fl_compress_threshold()` | Gradient sparsification |
| `eif_fl_add_noise()` | Differential privacy |
| `eif_fl_clip_gradients()` | Bounded sensitivity |
| `eif_fl_quantize_8bit()` | Communication compression |

---

## See Also

- [On-Device Learning](ON_DEVICE_LEARNING.md)
- [Model Evaluation](../guides/MODEL_CONVERSION_TUTORIAL.md)
- [Existing Demo](../../examples/el_demos/federated_learning/)
