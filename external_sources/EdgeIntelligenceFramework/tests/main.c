#include <stdio.h>
#include <string.h>

// Forward declarations of test suites
int run_matrix_tests(void);
int run_matrix_simd_tests(void);
int run_matrix_coverage_tests(void);
int run_matrix_fixed_tests(void);
int run_power_tests(void);
int run_core_fixed_point_tests(void);
int run_async_coverage_tests(void);
int run_memory_coverage_tests(void);
int run_dsp_tests(void);
int run_dsp_advanced_tests(void);
int run_iir_tests(void);
int run_pid_tests(void);
int run_smooth_tests(void);
int run_control_tests(void);
int run_filter_tests(void);
int run_neural_tests(void);
int run_neural_full_tests(void);
int run_neural_core_tests(void);
int run_nn_operators_tests(void);
int run_dispatcher_coverage_tests(void);
int run_serialization_tests(void);
int run_analysis_tests(void);
int run_bayesian_tests(void);
int run_slam_tests(void);
int run_ukf_slam_tests(void);
int run_imu_tests(void);
int run_hmm_tests(void);
int run_learning_tests(void);
int run_rl_tests(void);
int run_dqn_tests(void);
int run_audio_tests(void);
int run_ts_tests(void);
int run_ml_algorithms_tests(void);
int run_ml_classifier_tests(void);
int run_rf_tests(void);
int run_pca_tests(void);
int run_matrix_profile_tests(void);
int run_hal_mock_tests(void);
int run_hal_generic_tests(void);

// New test suites
int run_activity_tests(void);
int run_sensor_fusion_tests(void);
int run_ts_dtw_fixed_tests(void);
int run_fixed_point_tests(void);
int run_fft_fixed_tests(void);
int run_window_coverage_tests(void);
int run_transform_coverage_tests(void);
int run_dl_core_coverage_tests(void);
int run_layer_coverage_tests(void);
int run_dl_loader_coverage_tests(void);

int run_dl_simd_coverage_tests(void);
int run_audio_coverage_tests(void);
int run_audio_features_tests(void);
int run_new_features_tests(void);
int run_new_features_fixed_tests(void);
int run_ml_classifiers_fixed_tests(void);
int run_predictive_maintenance_tests(void);

// New ML header test suites
int run_rnn_tests(void);
int run_eval_tests(void);
int run_attention_unit_tests(void);
int run_ondevice_learning_tests(void);
int run_federated_tests(void);
int run_nlp_tests(void);
int run_nlp_coverage_tests(void);
int run_el_coverage_tests(void);

// Optimization header test suites
int run_assert_tests(void);
int run_simd_tests(void);
int run_async_tests(void);
int run_memory_guard_tests(void);
int run_logging_tests(void);
int run_da_coverage_tests(void);
int run_ml_knn_coverage_tests(void);
int run_rf_coverage_tests(void);
int run_bayes_coverage_tests(void);
int run_svm_coverage_tests(void);
int run_pca_coverage_tests(void);
int run_trees_coverage_tests(void);

int run_mp_fixed_tests(void);
int run_ts_hw_fixed_tests(void);

// Helper macro for suite selection
#define RUN_SUITE(name, func)                                                  \
  if (!suite || strcmp(suite, #func) == 0) {                                   \
    printf("\n=== Running Test Suite: %s ===\n", name);                        \
    if (func() != 0)                                                           \
      failed++;                                                                \
    if (suite)                                                                 \
      return failed;                                                           \
  }

int main(int argc, char *argv[]) {
  int failed = 0;
  const char *suite = NULL;

  // Parse --suite=name argument
  for (int i = 1; i < argc; i++) {
    if (strncmp(argv[i], "--suite=", 8) == 0) {
      suite = argv[i] + 8;
    }
  }

  if (!suite) {
    printf("Starting EIF Unit Tests (All Suites)...\n");
  } else {
    printf("Running Suite: %s\n", suite);
  }

  // Core tests
  RUN_SUITE("Matrix Operations", run_matrix_tests);
  RUN_SUITE("Matrix SIMD", run_matrix_simd_tests);
  RUN_SUITE("Matrix Coverage", run_matrix_coverage_tests);
  RUN_SUITE("Matrix Fixed", run_matrix_fixed_tests);
  RUN_SUITE("Power Profiler", run_power_tests);
  RUN_SUITE("Fixed Point Math", run_core_fixed_point_tests);
  RUN_SUITE("Async Coverage", run_async_coverage_tests);
  RUN_SUITE("Memory Coverage", run_memory_coverage_tests);

  // DSP tests
  RUN_SUITE("DSP Basic", run_dsp_tests);
  RUN_SUITE("DSP Advanced", run_dsp_advanced_tests);
  RUN_SUITE("IIR Filters", run_iir_tests);
  RUN_SUITE("PID Controller", run_pid_tests);
  RUN_SUITE("Smoothing", run_smooth_tests);
  RUN_SUITE("Control Systems", run_control_tests);
  RUN_SUITE("Filter Design", run_filter_tests);
  RUN_SUITE("Fixed Point (Q15)", run_fixed_point_tests);
  RUN_SUITE("Fixed Point FFT", run_fft_fixed_tests);
  RUN_SUITE("Window Functions", run_window_coverage_tests);
  RUN_SUITE("Transform Coverage", run_transform_coverage_tests);
  RUN_SUITE("Audio Coverage", run_audio_coverage_tests);
  RUN_SUITE("Audio Processing", run_audio_tests);
  RUN_SUITE("DSP Audio Features", run_audio_features_tests);
  RUN_SUITE("New Features (ML/CV/NLP)", run_new_features_tests);
  
  RUN_SUITE("New Features Fixed Point", run_new_features_fixed_tests);
  RUN_SUITE("ML Classifiers Fixed Point", run_ml_classifiers_fixed_tests);

  // Neural tests
  RUN_SUITE("Neural Basic", run_neural_tests);
  RUN_SUITE("Neural Full", run_neural_full_tests);
  RUN_SUITE("Neural Core Coverage", run_neural_core_tests);
  RUN_SUITE("DL SIMD Coverage", run_dl_simd_coverage_tests);
  RUN_SUITE("DL Loader Coverage", run_dl_loader_coverage_tests);
  RUN_SUITE("NN Operators", run_nn_operators_tests);  RUN_SUITE("Dispatcher Coverage", run_dispatcher_coverage_tests);  RUN_SUITE("Model Serialization", run_serialization_tests);

  // Analysis tests
  RUN_SUITE("Statistical Analysis", run_analysis_tests);
  RUN_SUITE("Learning Algorithms", run_learning_tests);
  RUN_SUITE("ML Algorithms", run_ml_algorithms_tests);
  RUN_SUITE("DA Coverage", run_da_coverage_tests);

  // ML tests
  RUN_SUITE("ML Classifiers", run_ml_classifier_tests);
  RUN_SUITE("Random Forest", run_rf_tests);
  RUN_SUITE("PCA", run_pca_tests);
  RUN_SUITE("Activity Recognition", run_activity_tests);
  RUN_SUITE("Sensor Fusion", run_sensor_fusion_tests);
  RUN_SUITE("Predictive Maintenance", run_predictive_maintenance_tests);

  // Data tests
  RUN_SUITE("Time Series", run_ts_tests);
  RUN_SUITE("DTW Fixed Point", run_ts_dtw_fixed_tests);
  RUN_SUITE("Matrix Profile", run_matrix_profile_tests);
  RUN_SUITE("Matrix Profile Fixed", run_mp_fixed_tests);
  RUN_SUITE("Holt Winters Fixed", run_ts_hw_fixed_tests);

  // Bayesian tests
  RUN_SUITE("Bayesian Inference", run_bayesian_tests);
  RUN_SUITE("SLAM", run_slam_tests);
  RUN_SUITE("UKF SLAM", run_ukf_slam_tests);
  RUN_SUITE("IMU Fusion", run_imu_tests);
  RUN_SUITE("HMM", run_hmm_tests);

  // RL tests
  RUN_SUITE("Reinforcement Learning", run_rl_tests);
  RUN_SUITE("DQN", run_dqn_tests);

  // HAL tests
  RUN_SUITE("HAL Mock", run_hal_mock_tests);
  RUN_SUITE("HAL Generic", run_hal_generic_tests);

  // New ML header tests
  RUN_SUITE("RNN/LSTM/GRU", run_rnn_tests);
  RUN_SUITE("Evaluation Metrics", run_eval_tests);
  RUN_SUITE("Attention Mechanisms", run_attention_unit_tests);
  RUN_SUITE("On-Device Learning", run_ondevice_learning_tests);
  RUN_SUITE("Federated Learning", run_federated_tests);
  RUN_SUITE("NLP", run_nlp_tests);
  RUN_SUITE("NLP Coverage", run_nlp_coverage_tests);
  RUN_SUITE("DL Core Coverage", run_dl_core_coverage_tests);
  RUN_SUITE("DL Layer Coverage", run_layer_coverage_tests);
  RUN_SUITE("EL Coverage", run_el_coverage_tests);


  // Optimization header tests
  RUN_SUITE("Assertions", run_assert_tests);
  RUN_SUITE("SIMD Operations", run_simd_tests);
  RUN_SUITE("Async Processing", run_async_tests);
  RUN_SUITE("Memory Guard", run_memory_guard_tests);
  RUN_SUITE("Logging System", run_logging_tests);
  RUN_SUITE("ML KNN Coverage", run_ml_knn_coverage_tests);
  RUN_SUITE("RF Coverage", run_rf_coverage_tests);
  RUN_SUITE("Bayes Coverage", run_bayes_coverage_tests);
  RUN_SUITE("SVM Coverage", run_svm_coverage_tests);
  RUN_SUITE("PCA Coverage", run_pca_coverage_tests);
  RUN_SUITE("Trees Coverage", run_trees_coverage_tests);

  // Summary
  if (!suite) {
    printf("\n========================================\n");
    if (failed == 0) {
      printf("ALL TESTS PASSED\n");
    } else {
      printf("%d TEST SUITES FAILED\n", failed);
    }
    printf("========================================\n");
  }

  return failed;
}
