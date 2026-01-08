add_executable(matrix_fixed_test main.c)

target_include_directories(matrix_fixed_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(matrix_fixed_test PRIVATE
    eif_core
    m
)
