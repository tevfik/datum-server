add_executable(run_tests
    main.c
    core/test_matrix.c
    core/test_matrix_simd.c
    core/test_matrix_coverage.c
    core/test_matrix_fixed.c
    core/test_power.c
    core/test_fixedpoint.c
    core/test_async_coverage.c
    core/test_memory_coverage.c
    core/test_assert.c
    core/test_async.c
    core/test_memory_guard.c
    core/test_logging.c
    dsp/test_dsp.c
    dsp/test_dsp_iir.c
    dsp/test_dsp_pid.c
    dsp/test_dsp_smooth.c
    dsp/test_dsp_control.c
    dsp/test_dsp_filters.c
    dsp/test_fixed_point.c
    dsp/test_fft_fixed.c
    dsp/test_window_coverage.c
    dsp/test_transform_coverage.c
    dsp/test_audio_coverage.c
    data/test_timeseries.c
    data/test_matrix_profile.c
    data/test_matrix_profile_fixed.c
    data/test_ts_dtw_fixed.c
    data/test_ts_hw_fixed.c
    dsp/test_audio.c
    dsp/test_audio_features.c
    integration/test_new_features.c
    integration/test_new_features_fixed.c
    integration/test_ml_classifiers_fixed.c
    dsp/test_dsp_advanced.c
    neural/test_neural.c
    neural/test_neural_full.c
    neural/test_neural_core_coverage.c
    neural/test_nn_operators.c
    dl/test_dl_simd_coverage.c
    dl/test_dl_core_coverage.c
    dl/test_layers_coverage.c
    dl/test_loader_coverage.c
    neural/test_dispatcher_coverage.c

    neural/test_serialization.c
    analysis/test_analysis.c
    analysis/test_learning.c
    analysis/test_ml_algorithms.c
    analysis/test_da_coverage.c
    ml/test_ml_classifiers.c
    ml/test_rf.c
    ml/test_pca.c
    ml/test_activity.c
    ml/test_sensor_fusion.c
    ml/test_predictive_maintenance.c
    ml/test_rnn.c
    ml/test_eval.c
    ml/test_ml_knn_coverage.c
    ml/test_rf_coverage.c
    ml/test_bayes_coverage.c
    ml/test_svm_coverage.c
    ml/test_pca_coverage.c
    ml/test_trees_coverage.c
    ml/test_attention.c
    ml/test_ondevice_learning.c
    ml/test_federated.c
    bayesian/test_bayesian.c
    bayesian/test_slam.c
    bayesian/test_ukf_slam.c
    bayesian/test_imu.c
    bayesian/test_hmm.c
    rl/test_rl.c
    rl/test_dqn.c
    nlp/test_nlp.c
    nlp/test_nlp_coverage.c
    el/test_el_coverage.c
    hal/test_hal_mock.c
    hal/test_hal_generic.c
    hal/test_simd.c
)


target_include_directories(run_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/framework
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dsp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dl/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../bf/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../da/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ml/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../el/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../nlp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../cv/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../hal/include
)

target_link_libraries(run_tests PRIVATE
    eif_core
    eif_dsp
    eif_dl
    eif_bf
    eif_da
    eif_ml
    eif_cv
    eif_el
    eif_nlp
    eif_hal
    m
)

# CTest Registration - Each test suite as separate CTest

# Core tests
add_test(NAME test_matrix COMMAND run_tests --suite=run_matrix_tests)
add_test(NAME test_matrix_simd COMMAND run_tests --suite=run_matrix_simd_tests)
add_test(NAME test_matrix_coverage COMMAND run_tests --suite=run_matrix_coverage_tests)
add_test(NAME test_matrix_fixed COMMAND run_tests --suite=run_matrix_fixed_tests)

# DSP tests
add_test(NAME test_dsp COMMAND run_tests --suite=run_dsp_tests)
add_test(NAME test_dsp_iir COMMAND run_tests --suite=run_iir_tests)
add_test(NAME test_dsp_pid COMMAND run_tests --suite=run_pid_tests)
add_test(NAME test_dsp_smooth COMMAND run_tests --suite=run_smooth_tests)
add_test(NAME test_dsp_control COMMAND run_tests --suite=run_control_tests)
add_test(NAME test_dsp_filters COMMAND run_tests --suite=run_filter_tests)
add_test(NAME test_fixed_point COMMAND run_tests --suite=run_fixed_point_tests)
add_test(NAME test_audio COMMAND run_tests --suite=run_audio_tests)
add_test(NAME test_dsp_advanced COMMAND run_tests --suite=run_dsp_advanced_tests)

# Neural tests
add_test(NAME test_neural COMMAND run_tests --suite=run_neural_tests)
add_test(NAME test_neural_full COMMAND run_tests --suite=run_neural_full_tests)
add_test(NAME test_neural_core_coverage COMMAND run_tests --suite=run_neural_core_tests)
add_test(NAME test_nn_operators COMMAND run_tests --suite=run_nn_operators_tests)
add_test(NAME test_dispatcher_coverage COMMAND run_tests --suite=run_dispatcher_coverage_tests)

# Analysis tests
add_test(NAME test_da_coverage COMMAND run_tests --suite=run_da_coverage_tests)
add_test(NAME test_serialization COMMAND run_tests --suite=run_serialization_tests)

# Analysis tests
add_test(NAME test_analysis COMMAND run_tests --suite=run_analysis_tests)
add_test(NAME test_learning COMMAND run_tests --suite=run_learning_tests)
add_test(NAME test_ml_algorithms COMMAND run_tests --suite=run_ml_algorithms_tests)

# ML tests
add_test(NAME test_ml_classifiers COMMAND run_tests --suite=run_ml_classifier_tests)
add_test(NAME test_activity COMMAND run_tests --suite=run_activity_tests)
add_test(NAME test_sensor_fusion COMMAND run_tests --suite=run_sensor_fusion_tests)
add_test(NAME test_predictive_maintenance COMMAND run_tests --suite=run_predictive_maintenance_tests)

# New ML header tests
add_test(NAME test_rnn COMMAND run_tests --suite=run_rnn_tests)
add_test(NAME test_eval COMMAND run_tests --suite=run_eval_tests)
add_test(NAME test_attention COMMAND run_tests --suite=run_attention_unit_tests)
add_test(NAME test_ondevice_learning COMMAND run_tests --suite=run_ondevice_learning_tests)
add_test(NAME test_federated COMMAND run_tests --suite=run_federated_tests)
add_test(NAME test_nlp COMMAND run_tests --suite=run_nlp_tests)

# Data tests
add_test(NAME test_timeseries COMMAND run_tests --suite=run_ts_tests)
add_test(NAME test_matrix_profile COMMAND run_tests --suite=run_matrix_profile_tests)

# Bayesian tests
add_test(NAME test_bayesian COMMAND run_tests --suite=run_bayesian_tests)
add_test(NAME test_slam COMMAND run_tests --suite=run_slam_tests)
add_test(NAME test_ukf_slam COMMAND run_tests --suite=run_ukf_slam_tests)
add_test(NAME test_imu COMMAND run_tests --suite=run_imu_tests)

# RL tests
add_test(NAME test_rl COMMAND run_tests --suite=run_rl_tests)
add_test(NAME test_dqn COMMAND run_tests --suite=run_dqn_tests)

# HAL tests
add_test(NAME test_hal_mock COMMAND run_tests --suite=run_hal_mock_tests)

# Optimization header tests
add_test(NAME test_assert COMMAND run_tests --suite=run_assert_tests)
add_test(NAME test_simd COMMAND run_tests --suite=run_simd_tests)
add_test(NAME test_async COMMAND run_tests --suite=run_async_tests)
add_test(NAME test_memory_guard COMMAND run_tests --suite=run_memory_guard_tests)
add_test(NAME test_logging COMMAND run_tests --suite=run_logging_tests)

# Run all tests (default)
add_test(NAME eif_all_tests COMMAND run_tests)

add_executable(test_quantization test_quantization.c)
target_link_libraries(test_quantization PRIVATE eif_dl eif_core m)
target_include_directories(test_quantization PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dl/src
)
add_test(NAME test_quantization COMMAND test_quantization)
add_subdirectory(dl)

add_executable(test_fixedpoint_accuracy core/test_fixedpoint_accuracy.c)
target_link_libraries(test_fixedpoint_accuracy PRIVATE eif_core eif_dsp m)
target_include_directories(test_fixedpoint_accuracy PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/framework
)
add_test(NAME test_fixedpoint_accuracy COMMAND test_fixedpoint_accuracy)

# Opus Integration Test
add_executable(test_opus_integration
    dsp/test_opus_integration.c
)
target_link_libraries(test_opus_integration PRIVATE eif_core eif_dsp m)
target_include_directories(test_opus_integration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/framework
)
add_test(NAME test_opus_integration COMMAND test_opus_integration)

add_executable(test_dsp_fixed dsp/test_dsp_fixed.c)
target_link_libraries(test_dsp_fixed PRIVATE eif_core eif_dsp m)
target_include_directories(test_dsp_fixed PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dsp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/framework
)
add_test(NAME test_dsp_fixed COMMAND test_dsp_fixed)
