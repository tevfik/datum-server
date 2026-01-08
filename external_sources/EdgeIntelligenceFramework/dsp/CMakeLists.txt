# Digital Signal Processing Library

add_library(eif_dsp STATIC
    src/eif_dsp_transform.c
    src/eif_dsp_filter.c
    src/eif_dsp_audio.c
    src/eif_dsp_features.c
    src/eif_dsp_window.c
    src/eif_dsp_fixed.c
    src/eif_dsp_advanced.c
    src/eif_dsp_simd.c
    src/eif_dsp_iir.c
    src/eif_dsp_pid.c
    src/eif_dsp_smooth.c
    src/eif_dsp_resample.c
    src/eif_dsp_agc.c
    src/eif_dsp_beamformer.c
    src/eif_dsp_generator.c
    src/eif_dsp_peak.c
    src/eif_ringbuffer.c
    src/eif_adpcm.c
    src/eif_vad_advanced.c
    src/eif_dsp_fft_fixed.c
    src/eif_dsp_pid_fixed.c
    src/eif_dsp_agc_fixed.c
    src/eif_dsp_smooth_fixed.c
    src/eif_dsp_resample_fixed.c
    src/eif_dsp_beamformer_fixed.c
    src/eif_opus.c
)

option(EIF_USE_OPUS "Enable Opus Audio Codec Support" ON)

if(EIF_USE_OPUS)
    add_definitions(-DEIF_USE_OPUS)
    target_include_directories(eif_dsp PUBLIC third_party/libopus/include)
    
    # In a real scenario, we would add all libopus sources here or link a static lib
    # For now, we add our mock implementation
    target_sources(eif_dsp PRIVATE third_party/libopus/src/mock_opus.c)

    # If building for ESP32 or Embedded without FPU
    option(EIF_OPUS_FIXED_POINT "Use Fixed Point Opus" OFF)
    if(EIF_OPUS_FIXED_POINT)
        add_definitions(-DFIXED_POINT)
    endif()
endif()

include(CheckCCompilerFlag)
check_c_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
if(COMPILER_SUPPORTS_AVX2)
    target_compile_options(eif_dsp PRIVATE "-mavx2")
endif()

check_c_compiler_flag("-mfma" COMPILER_SUPPORTS_FMA)
if(COMPILER_SUPPORTS_FMA)
    target_compile_options(eif_dsp PRIVATE "-mfma")
endif()

target_include_directories(eif_dsp PUBLIC
    include
)

target_link_libraries(eif_dsp PUBLIC eif_core m)
