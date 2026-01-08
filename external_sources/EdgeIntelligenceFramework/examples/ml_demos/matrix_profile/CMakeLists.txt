add_executable(matrix_profile_demo main.c)
target_link_libraries(matrix_profile_demo PRIVATE eif_da eif_dsp eif_core m)
target_include_directories(matrix_profile_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../data-analysis/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../signal-processing/include
)
