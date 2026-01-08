add_executable(kalman_fixed_test main.c)

target_include_directories(kalman_fixed_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(kalman_fixed_test PRIVATE
    eif_core
    eif_bf
    m
)
