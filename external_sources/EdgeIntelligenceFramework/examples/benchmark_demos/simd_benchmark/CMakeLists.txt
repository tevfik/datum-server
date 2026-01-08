add_executable(simd_benchmark main.c)

target_include_directories(simd_benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../hal/include
)

target_link_libraries(simd_benchmark PRIVATE
    eif_core
    eif_hal
    m
)

# Set output directory
set_target_properties(simd_benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
