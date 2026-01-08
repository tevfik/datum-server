# Edge Intelligence Framework - Algorithm Reference

Complete reference of all algorithms available in the EIF library.

---

## DSP Algorithms

### Smoothing & Noise Reduction (`eif_dsp_smooth.h`)

| Algorithm | Function | Description | Memory |
|-----------|----------|-------------|--------|
| **EMA** | `eif_ema_update()` | Exponential Moving Average for sensor smoothing | 8 bytes |
| **Median Filter** | `eif_median_update()` | Removes impulse noise while preserving edges | 36 bytes |
| **Moving Average** | `eif_ma_update()` | Simple sliding window average | 72 bytes |
| **Rate Limiter** | `eif_rate_limiter_update()` | Limits signal slew rate (servo control) | 12 bytes |
| **Hysteresis** | `eif_hysteresis_update()` | Schmitt trigger for clean switching | 12 bytes |
| **Debounce** | `eif_debounce_update()` | Button input filtering | 12 bytes |

### Control Utilities (`eif_dsp_control.h`)

| Algorithm | Function | Description | Memory |
|-----------|----------|-------------|--------|
| **Deadzone** | `eif_deadzone_apply()` | Joystick centering | 8 bytes |
| **Differentiator** | `eif_differentiator_update()` | Rate of change detection | 12 bytes |
| **Integrator** | `eif_integrator_update()` | Accumulation with anti-windup | 24 bytes |
| **Zero-Crossing** | `eif_zero_cross_update()` | Frequency/phase detection | 16 bytes |
| **Peak Detector** | `eif_peak_detector_update()` | Envelope following | 12 bytes |

### IIR Filters (`eif_dsp_iir.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Biquad Filter** | `eif_iir_process()` | Second-order IIR filter |
| **Lowpass** | `eif_iir_lowpass()` | Butterworth lowpass |
| **Highpass** | `eif_iir_highpass()` | Butterworth highpass |
| **Bandpass** | `eif_iir_bandpass()` | Band-pass filter |

### FIR Filters (`eif_dsp_fir.h`) ✨NEW

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **FIR Filter** | `eif_fir_process()` | Finite Impulse Response filter |
| **Block Processing** | `eif_fir_process_block()` | Process sample blocks |
| **Lowpass Design** | `eif_fir_design_lowpass()` | Windowed-sinc lowpass |
| **Highpass Design** | `eif_fir_design_highpass()` | Spectral inversion highpass |
| **Window Functions** | `eif_window_value()` | Hamming, Hanning, Blackman |

### Biquad Cascade (`eif_dsp_biquad.h`) ✨NEW

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Biquad Section** | `eif_biquad_process()` | Direct Form II Transposed |
| **Cascade** | `eif_biquad_cascade_process()` | Multi-stage filtering |
| **Lowpass** | `eif_biquad_lowpass()` | 2nd order lowpass |
| **Highpass** | `eif_biquad_highpass()` | 2nd order highpass |
| **Bandpass** | `eif_biquad_bandpass()` | Band-pass filter |
| **Notch** | `eif_biquad_notch()` | Band-reject filter |
| **Peaking EQ** | `eif_biquad_peaking()` | Parametric EQ band |
| **Low Shelf** | `eif_biquad_lowshelf()` | Bass boost/cut |
| **High Shelf** | `eif_biquad_highshelf()` | Treble boost/cut |
| **Allpass** | `eif_biquad_allpass()` | Phase shifting |
| **Butterworth 4th** | `eif_biquad_butter4_lowpass()` | 2-stage cascade |
| **Butterworth 6th** | `eif_biquad_butter6_lowpass()` | 3-stage cascade |

### Fixed-Point Filters ✨NEW

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Q15 FIR** | `eif_fir_q15_process()` | 16-bit signed FIR |
| **Q15 Biquad** | `eif_biquad_q15_process()` | 16-bit IIR section |
| **Q15 Cascade** | `eif_biquad_q15_cascade_process()` | Multi-stage Q15 |

### PID Controller (`eif_dsp_pid.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **PID** | `eif_pid_update()` | Full PID with anti-windup |
| **PI** | `eif_pid_update()` | Proportional-Integral only |
| **P** | `eif_pid_update()` | Proportional only |

### FFT & Transforms (`eif_dsp_fft.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **FFT** | `eif_fft()` | Fast Fourier Transform |
| **IFFT** | `eif_ifft()` | Inverse FFT |
| **RFFT** | `eif_rfft()` | Real-valued FFT |
| **STFT** | `eif_stft()` | Short-Time Fourier Transform |

### Audio Processing (`eif_audio.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **MFCC** | `eif_mfcc_compute()` | Mel-Frequency Cepstral Coefficients |
| **Mel Filterbank** | `eif_mel_filterbank()` | Mel-scale filtering |
| **Pre-emphasis** | `eif_preemphasis()` | High-frequency boost |
| **VAD** | `eif_vad()` | Voice Activity Detection |

---

## Machine Learning Algorithms

### Neural Networks (`eif_neural.h`)

| Layer Type | Function | Description |
|------------|----------|-------------|
| **Dense** | `eif_nn_dense()` | Fully connected layer |
| **Conv2D** | `eif_nn_conv2d()` | 2D convolution |
| **Conv1D** | `eif_nn_conv1d()` | 1D convolution |
| **MaxPool2D** | `eif_nn_maxpool2d()` | 2D max pooling |
| **Flatten** | `eif_nn_flatten()` | Tensor reshape |
| **ReLU** | `eif_nn_relu()` | ReLU activation |
| **Softmax** | `eif_nn_softmax()` | Softmax activation |
| **Sigmoid** | `eif_nn_sigmoid()` | Sigmoid activation |

### Recurrent Networks (`eif_nn_rnn.h`) ✨NEW

| Layer Type | Function | Description |
|------------|----------|-------------|
| **GRU Cell** | `eif_gru_forward()` | Gated Recurrent Unit |
| **LSTM Cell** | `eif_lstm_forward()` | Long Short-Term Memory |

### Attention Mechanism (`eif_nn_attention.h`) ✨NEW

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Scaled Dot-Product** | `eif_attention_forward()` | Core attention |
| **Multi-Head** | `eif_multihead_init()` | Multi-head attention |
| **Position Encoding** | `eif_position_encoding()` | Sinusoidal encoding |
| **Layer Norm** | `eif_layer_norm()` | Normalization |

### Classical ML (`eif_ml.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **K-NN** | `eif_knn_classify()` | K-Nearest Neighbors |
| **K-Means** | `eif_kmeans_fit()` | Clustering |
| **PCA** | `eif_pca_fit()` | Dimensionality reduction |
| **Linear Regression** | `eif_linear_fit()` | Linear model |
| **Decision Stump** | `eif_stump_predict()` | Single split classifier |

### SVM (`eif_ml_svm.h`) ✨NEW

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Linear SVM** | `eif_linear_svm_predict()` | Linear classifier |
| **RBF SVM** | `eif_rbf_svm_predict()` | Kernel SVM |

### Naive Bayes (`eif_ml_naive_bayes.h`) ✨NEW

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Gaussian NB** | `eif_gaussian_nb_predict()` | Continuous features |
| **Multinomial NB** | `eif_multinomial_nb_predict()` | Text/count data |

### Logistic Regression (`eif_ml_logistic.h`) ✨NEW

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Binary Logistic** | `eif_binary_logistic_predict()` | Binary classification |
| **Softmax Regression** | `eif_softmax_regression_predict()` | Multi-class |

### Random Forest (`eif_ml_random_forest.h`) ✨NEW

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Decision Tree** | `eif_dt_predict()` | Single tree inference |
| **Random Forest** | `eif_rf_predict()` | Ensemble (majority voting) |
| **Probabilities** | `eif_rf_predict_proba()` | Vote proportions |

### Gradient Boosting (`eif_ml_gradient_boost.h`) ✨NEW

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **GBM Tree** | `eif_gbm_tree_predict()` | Single boosted tree |
| **GBM Predict** | `eif_gbm_predict()` | Additive model inference |
| **Probability** | `eif_gbm_predict_proba_binary()` | Binary probability |

### Anomaly Detection (`eif_da.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Z-Score** | `eif_zscore()` | Statistical outlier detection |
| **Isolation Forest** | `eif_isolation_forest()` | Tree-based anomaly detection |
| **LOF** | `eif_lof()` | Local Outlier Factor |

---

## NLP & Speech (`nlp/`) ✨NEW

### Tokenizer (`eif_nlp_tokenizer.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Tokenize** | `eif_tokenizer_encode()` | Text to token IDs |
| **Vocabulary** | `eif_tokenizer_add_word()` | Build vocabulary |

### Phoneme Recognition (`eif_nlp_phoneme.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Phoneme Match** | `eif_phoneme_recognize()` | Edit-distance matching |
| **Common Commands** | `eif_phoneme_setup_common_commands()` | YES/NO/STOP/GO |

---

## Bayesian Filters & State Estimation

### Kalman Filters (`eif_bayesian.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Kalman Filter** | `eif_kalman_update()` | Linear state estimation |
| **EKF** | `eif_ekf_update()` | Extended Kalman Filter |
| **UKF** | `eif_ukf_update()` | Unscented Kalman Filter |
| **Particle Filter** | `eif_particle_filter()` | Monte Carlo estimation |

### Sensor Fusion

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Complementary Filter** | `eif_complementary()` | IMU fusion |
| **Madgwick Filter** | `eif_madgwick()` | AHRS quaternion |
| **Mahony Filter** | `eif_mahony()` | AHRS with PI correction |

---

## Reinforcement Learning (`eif_rl.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Q-Learning** | `eif_ql_update()` | Tabular Q-learning |
| **DQN** | `eif_dqn_update()` | Deep Q-Network |
| **SARSA** | `eif_sarsa_update()` | On-policy TD learning |

---

## Edge Learning (`eif_el.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Online Learning** | `eif_online_fit()` | Incremental model update |
| **Transfer Learning** | `eif_transfer()` | Fine-tuning pretrained models |
| **Few-Shot** | `eif_fewshot()` | Learning from few examples |

---

## Data Analysis (`eif_da.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Matrix Profile** | `eif_matrix_profile()` | Time series motif discovery |
| **DTW** | `eif_dtw()` | Dynamic Time Warping |
| **Autocorrelation** | `eif_autocorr()` | Signal periodicity |
| **Cross-correlation** | `eif_xcorr()` | Signal similarity |

---

## Edge Intelligence ✨NEW

### Model Quantization (`eif_quantize.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **INT8 Symmetric** | `eif_quant_to_int8_sym()` | Symmetric 8-bit quantization |
| **INT8 Asymmetric** | `eif_quant_to_int8_asym()` | Full-range asymmetric |
| **Q15** | `eif_quant_to_q15()` | 16-bit fixed-point |
| **Calibration** | `eif_quant_stats_update()` | Collect range statistics |
| **Quality** | `eif_quant_sqnr()` | Signal-to-noise ratio |

### Adaptive Algorithms (`eif_adaptive_threshold.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Z-Score Threshold** | `eif_z_threshold_check()` | Self-tuning outlier detection |
| **Percentile Threshold** | `eif_percentile_threshold_check()` | Sliding window percentiles |
| **Drift Detection** | `eif_drift_update()` | Concept drift monitoring |
| **Running Stats** | `eif_running_stats_update()` | Exponential mean/variance |

### Sensor Fusion (`eif_sensor_fusion.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Complementary Filter** | `eif_complementary_update()` | IMU rate+angle fusion |
| **Weighted Fusion** | `eif_weighted_fusion_compute()` | Confidence-weighted sensors |
| **Sensor Voting** | `eif_voting_compute()` | Fault-tolerant voting |
| **1D Kalman** | `eif_kalman_1d_update()` | Simple sensor fusion |

### Online Learning (`eif_online_learning.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Online Linear** | `eif_online_linear_update()` | SGD regression/classification |
| **Online Centroid** | `eif_online_centroid_predict()` | Nearest class mean |
| **Online Normalizer** | `eif_online_normalizer_transform()` | Streaming z-score |
| **Replay Buffer** | `eif_replay_buffer_sample()` | Experience replay |

### Edge Inference (`eif_edge_inference.h`)

| Algorithm | Function | Description |
|-----------|----------|-------------|
| **Model Profiling** | `eif_model_profile()` | FLOPS/memory estimation |
| **Memory Estimate** | `eif_estimate_memory()` | RAM/Flash requirements |
| **Fits Memory** | `eif_model_fits_memory()` | Target compatibility check |

---

## Usage Examples

### Smooth a noisy sensor
```c
#include "eif_dsp_smooth.h"

eif_ema_t ema;
eif_ema_init(&ema, 0.2f);  // alpha = 0.2

float smoothed = eif_ema_update(&ema, raw_reading);
```

### Debounce a button
```c
#include "eif_dsp_smooth.h"

eif_debounce_t db;
eif_debounce_init(&db, 5);  // 5 consecutive samples

bool stable = eif_debounce_update(&db, button_raw);
```

### Remove joystick center jitter
```c
#include "eif_dsp_control.h"

eif_deadzone_t dz;
eif_deadzone_init(&dz, 0.1f);  // 10% deadzone

float clean = eif_deadzone_apply(&dz, joystick_x);
```

---

## Building Documentation

```bash
make docs
```

This generates HTML documentation in `docs/site/`.
