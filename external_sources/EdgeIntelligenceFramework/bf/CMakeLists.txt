# Bayesian Filters Library

add_library(eif_bf STATIC
    src/eif_bayesian.c
    src/eif_bayesian_fixed.c
    src/eif_slam.c
    src/eif_imu.c
    src/eif_bf_hmm.c
)

target_include_directories(eif_bf PUBLIC
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dsp/include
)

target_link_libraries(eif_bf PUBLIC eif_core)

# Backward compatibility alias
add_library(eif_bayesian ALIAS eif_bf)
