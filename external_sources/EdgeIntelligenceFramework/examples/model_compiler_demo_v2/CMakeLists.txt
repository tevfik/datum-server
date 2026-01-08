
cmake_minimum_required(VERSION 3.10)
project(model_compiler_demo_v2)

# Assuming we are in examples/model_compiler_demo_v2
# Root is ../..

set(EIF_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
include_directories(${EIF_ROOT}/core/include)
include_directories(${EIF_ROOT}/ml/include)
include_directories(${EIF_ROOT}/dsp/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Sources
set(SOURCES
    main.c
    ${EIF_ROOT}/core/src/eif_fixedpoint.c
    ${EIF_ROOT}/core/src/eif_memory.c
)

add_executable(model_compiler_demo_v2 ${SOURCES})

# math lib
target_link_libraries(model_compiler_demo_v2 m)
