# Federated Learning Tutorial: Privacy-Preserving Distributed ML

## Learning Objectives

- Understand federated learning architecture
- Implement FedAvg algorithm
- Train models without sharing raw data
- Deploy on multiple ESP32 clients

**Level**: Intermediate  
**Time**: 45 minutes

---

## 1. What is Federated Learning?

### Traditional ML vs Federated

```
Traditional:                  Federated:
                              
Data → [Server] → Model      Device₁ ──┐
                              Device₂ ──┼→ [Aggregator] → Global Model
                              Device₃ ──┘
                              
Data leaves device!           Data stays on device!
```

### Privacy Benefits
- Raw data never leaves devices
- Only model updates shared
- GDPR/HIPAA compliant by design

---

## 2. FedAvg Algorithm

```
For each round r = 1, 2, ...:
  1. Server sends global model W to clients
  2. Each client k:
     - Trains on local data: Wₖ = W - η∇L(W)
     - Sends update ΔWₖ = Wₖ - W to server
  3. Server aggregates:
     W_new = W + Σ(nₖ/n) × ΔWₖ
```

---

## 3. EIF Implementation

```c
// Server side
eif_federated_server_t server;
eif_federated_server_init(&server, model_size, num_clients, &pool);

// Aggregate client updates
for (int c = 0; c < num_clients; c++) {
    eif_federated_server_add_update(&server, client_updates[c], client_sizes[c]);
}
eif_federated_server_aggregate(&server, global_weights);

// Client side
eif_federated_client_t client;
eif_federated_client_init(&client, model_size, learning_rate, &pool);

// Local training
eif_federated_client_train(&client, local_data, labels, n_samples);
eif_federated_client_get_update(&client, weight_update);
```

---

## 4. ESP32 Multi-Device Setup

```c
// On each ESP32 client
void federated_task(void* arg) {
    // Connect to server
    wifi_connect();
    
    while (1) {
        // Receive global model
        receive_model(global_weights);
        eif_federated_client_set_weights(&client, global_weights);
        
        // Train on local sensor data
        collect_local_data(local_data, &n_samples);
        eif_federated_client_train(&client, local_data, labels, n_samples);
        
        // Send update
        eif_federated_client_get_update(&client, update);
        send_update_to_server(update);
        
        vTaskDelay(60000 / portTICK_PERIOD_MS);  // Every minute
    }
}
```

---

## 5. Summary

### Key APIs
- `eif_federated_server_init()` - Initialize aggregator
- `eif_federated_client_train()` - Local training
- `eif_federated_server_aggregate()` - FedAvg

### Next: `ewc_learning` for continual learning
