add_executable(matrix_test main.c)

target_include_directories(matrix_test PRIVATE 
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/signal-processing/include
)

target_link_libraries(matrix_test PRIVATE
    eif_core
    m
)
