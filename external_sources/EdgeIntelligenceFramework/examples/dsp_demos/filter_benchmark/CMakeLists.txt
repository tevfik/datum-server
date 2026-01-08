add_executable(filter_benchmark main.c)

target_include_directories(filter_benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../dsp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
)

target_link_libraries(filter_benchmark PRIVATE eif_dsp eif_core m)
