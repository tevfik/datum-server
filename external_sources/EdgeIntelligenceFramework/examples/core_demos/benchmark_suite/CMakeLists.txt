add_executable(benchmark_suite main.c)

target_include_directories(benchmark_suite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../dsp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../ml/include
)

target_link_libraries(benchmark_suite PRIVATE eif_core eif_dsp eif_ml m)
