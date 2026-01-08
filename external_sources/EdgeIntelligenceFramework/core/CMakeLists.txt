add_library(eif_core STATIC
    src/eif_memory.c
    src/eif_fixedpoint.c
    src/eif_matrix.c
    src/eif_matrix_fixed.c
    src/eif_matrix_ops_simd.c
    src/eif_power.c
    src/eif_async.c
    src/eif_logging.c
)

include(CheckCCompilerFlag)
check_c_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
if(COMPILER_SUPPORTS_AVX2)
    target_compile_options(eif_core PRIVATE "-mavx2")
endif()

check_c_compiler_flag("-mfma" COMPILER_SUPPORTS_FMA)
if(COMPILER_SUPPORTS_FMA)
    target_compile_options(eif_core PRIVATE "-mfma")
endif()

target_include_directories(eif_core PUBLIC
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dsp/include
)

target_link_libraries(eif_core PUBLIC m)
