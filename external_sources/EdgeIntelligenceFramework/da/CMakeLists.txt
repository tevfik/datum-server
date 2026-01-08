# Data Analysis Library

add_library(eif_da STATIC
    src/eif_data_analysis.c
    src/eif_learning.c
    src/eif_ts_arima.c
    src/eif_ts_hw.c
    src/eif_ts_extensions.c
    src/eif_matrix_profile.c
    src/eif_anomaly.c
    src/eif_ts_dtw.c
    src/eif_ts_dtw_fixed.c
    src/eif_matrix_profile_fixed.c
    src/eif_ts_hw_fixed.c
)

target_include_directories(eif_da PUBLIC
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dsp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ml/include
)

target_link_libraries(eif_da PUBLIC eif_core eif_ml)

# Backward compatibility alias
add_library(eif_data_analysis ALIAS eif_da)
