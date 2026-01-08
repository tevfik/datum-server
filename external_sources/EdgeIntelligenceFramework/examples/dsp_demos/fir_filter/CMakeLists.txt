add_executable(fir_filter_demo main.c)

target_include_directories(fir_filter_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../dsp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
)

target_link_libraries(fir_filter_demo PRIVATE eif_dsp eif_core m)
