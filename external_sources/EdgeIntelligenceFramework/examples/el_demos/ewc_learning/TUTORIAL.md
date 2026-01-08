# EWC Tutorial: Continual Learning Without Forgetting

## Learning Objectives

- Understand catastrophic forgetting
- Implement Elastic Weight Consolidation (EWC)
- Train on sequential tasks without losing old knowledge

**Level**: Intermediate to Advanced  
**Time**: 30 minutes

---

## 1. The Problem: Catastrophic Forgetting

```
Train Task A → 95% accuracy on A
Train Task B → 90% on B, but 20% on A!  ← Forgetting!
```

Standard neural networks overwrite old knowledge.

---

## 2. EWC Solution

**Key Idea**: Some weights are more important than others!

```
Loss = L_new(θ) + λ × Σᵢ Fᵢ(θᵢ - θ*ᵢ)²

where:
  Fᵢ = Fisher Information (importance of weight i)
  θ* = Optimal weights for old task
  λ  = Regularization strength
```

---

## 3. EIF Implementation

```c
// Initialize EWC context
eif_ewc_context_t ewc;
eif_ewc_init(&ewc, 
    num_weights,       // Total model weights
    learning_rate,     // 0.001
    ewc_lambda,        // 1000.0 (importance)
    &pool);

// After training Task A:
eif_ewc_compute_fisher(&ewc, data_A, labels_A, n_samples_A);
eif_ewc_save_optimal_weights(&ewc, weights);

// Training Task B with EWC:
for (int epoch = 0; epoch < 100; epoch++) {
    for (int i = 0; i < n_samples_B; i++) {
        eif_ewc_train_step(&ewc, x[i], y[i]);
    }
}
// Now model works on BOTH A and B!
```

---

## 4. Visualizing Fisher Information

```
Weight Index:    0    1    2    3    4    5
Fisher Value:  [███ ][█  ][████][██ ][█  ][███ ]
               High  Low  High  Med  Low  High
               
High Fisher = Important for old task = Constrained
Low Fisher = Can be freely modified for new task
```

---

## 5. Summary

### Key APIs
- `eif_ewc_init()` - Initialize EWC
- `eif_ewc_compute_fisher()` - Calculate importance
- `eif_ewc_train_step()` - Train with regularization

### Use Cases
- IoT devices learning new patterns over time
- Personalization without forgetting base model
