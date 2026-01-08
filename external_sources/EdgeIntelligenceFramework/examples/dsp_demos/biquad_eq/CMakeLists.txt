add_executable(biquad_eq_demo main.c)

target_include_directories(biquad_eq_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../dsp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
)

target_link_libraries(biquad_eq_demo PRIVATE eif_dsp eif_core m)
