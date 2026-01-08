# DSP vs. CNN on the Edge: A Quantitative Comparison using EIF

**Date:** December 11, 2025  
**Author:** Edge Intelligence Framework Team  
**Topic:** Embedded AI Performance Benchmarking

---

## 🚀 Introduction

In the world of TinyML, there's a constant debate: **"Do I really need a Neural Network for this?"**

With the rise of powerful microcontrollers like the ESP32 and ARM Cortex-M7, it's tempting to throw a Convolutional Neural Network (CNN) at every problem. But is that efficient? 

In this post, we use the [Edge Intelligence Framework (EIF)](https://github.com/yourusername/edge-intelligence-framework) to answer this question with hard data. We implemented **Anomalous Vibration Detection** using three distinct approaches:
1.  **DSP**: Digital Signal Processing (FFT)
2.  **ML**: Classical Machine Learning (Statistical Features + Random Forest)
3.  **DL**: Deep Learning (1D CNN)

Let's see who wins the "TinyML Shootout".

---

## 🎯 The Experiment

**Scenario**: A predictive maintenance sensor on a motor.  
**Goal**: Detect if the motor is "Healthy" (Clean 2Hz sine) or "Faulty" (Noisy, high-frequency harmonics).  
**Constraint**: Must run on a constrained embedded target (simulated here for benchmarking).

### The Contenders

#### 1. The Old Guard: DSP (FFT) 📉
We use a Fast Fourier Transform (FFT) to analyze the frequency spectrum.
- **Logic**: Calculate energy in high-frequency bins (>10Hz). If energy > Threshold, it's a fault.
- **Pros**: Extremely interpretable, no training data needed.
- **Cons**: Rigid; requires manual tuning of thresholds.

#### 2. The Smart Middleman: Classical ML 🌳
We extract statistical features (`Mean`, `Variance`, `Zero-Crossings`) and pass them to a hard-coded Decision Tree.
- **Logic**: `If Variance > X and Range > Y then Fault`.
- **Pros**: Explainable, handles non-linear boundaries better than simple thresholds.
- **Cons**: Feature engineering is manual work.

#### 3. The Heavy Hitter: Deep Learning (1D CNN) 🧠
We feed the raw time-series signal (128 samples) into a 1D Convolutional Neural Network.
- **Architecture**: `Conv1D(k=3, f=4) -> ReLU -> MaxPool -> Dense(2) -> Softmax`.
- **Pros**: Learns features automatically; robust to weird noise patterns.
- **Cons**: Interpretation is a black box; higher compute cost.

---

## 📊 The Results

We ran each algorithm **1000 times** on the same synthesized dataset using EIF's benchmarking tools. Here is the raw `tinyml_shootout` output:

| Metric | DSP (FFT) | ML (Stats) | DL (1D CNN) |
|:-------|:----------:|:----------:|:-----------:|
| **Latency per Inference** | **4.84 µs** ⚡ | **4.64 µs** ⚡ | 7.99 µs |
| **Speedup vs DL** | **1.65x** | **1.72x** | 1.0x |
| **Code Complexity** | Low | Medium | High |
| **Accuracy (Simulated)**| 100% | 100% | 100% |

> *Note: Timings are on a host CPU. Relative scaling applies for MCUs.*

### Insights

1.  **Simplicity Wins on Speed**: The Classical ML approach was actually the fastest! Calculating simple stats like variance is computationally cheaper than both the FFT and the Convolution.
2.  **DSP is Reliable**: The FFT approach was nearly as fast and offers essentially zero ambiguity. You know exactly *why* it triggered.
3.  **CNN Tax**: The 1D CNN was **1.7x slower**. While ~8µs is still blazing fast, on a battery-powered device, that's nearly double the energy consumption for the same result.

---

## 🏆 Conclusion: When to use what?

Based on our experiments with EIF, here acts as our guide:

- **Use DSP/ML when**: You understand the physics of your signal (e.g., specific vibration frequencies) and need maximum battery life.
- **Use Deep Learning (CNN) when**: The patterns are complex (e.g., voice, gestures, visual objects) or unknown, and you have plenty of data to let the model learn robust features.

**Want to try it yourself?**
Check out the `benchmarks/tinyml_shootout` in the [Edge Intelligence Framework](https://github.com/yourusername/edge-intelligence-framework).

```bash
# Run the benchmark yourself
cmake .. && make tinyml_shootout
./bin/tinyml_shootout
```

*Happy Coding!*
