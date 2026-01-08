# Deep Learning Library

add_library(eif_dl STATIC
    src/eif_dl_core.c
    src/eif_dl_context.c
    src/eif_dl_layers.c
    src/eif_dl_model_loader.c
    src/eif_dl_train.c
    src/eif_dl_activations.c
    src/eif_dl_rnn.c
    src/eif_dl_ops_simd.c
    src/eif_dl_im2col.c
    src/eif_dl_quantization.c
)

include(CheckCCompilerFlag)
check_c_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
if(COMPILER_SUPPORTS_AVX2)
    target_compile_options(eif_dl PRIVATE "-mavx2")
endif()

check_c_compiler_flag("-mfma" COMPILER_SUPPORTS_FMA)
if(COMPILER_SUPPORTS_FMA)
    target_compile_options(eif_dl PRIVATE "-mfma")
endif()

target_include_directories(eif_dl PUBLIC
    include
)

target_link_libraries(eif_dl PUBLIC eif_core)

# Backward compatibility alias
add_library(eif_neural ALIAS eif_dl)
