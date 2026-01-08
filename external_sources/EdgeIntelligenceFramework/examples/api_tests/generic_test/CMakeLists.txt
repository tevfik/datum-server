cmake_minimum_required(VERSION 3.10)
project(generic_test)

add_executable(generic_test main.c)

target_include_directories(generic_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/signal-processing/include
    ${CMAKE_SOURCE_DIR}/bayesian-filters/include
)

target_link_libraries(generic_test PRIVATE eif_core eif_dsp eif_bf m)
