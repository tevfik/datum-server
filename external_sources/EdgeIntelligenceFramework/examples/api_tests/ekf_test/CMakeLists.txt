add_executable(ekf_test main.c)

target_include_directories(ekf_test PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/signal-processing/include
)

target_link_libraries(ekf_test PRIVATE
    eif_core
    eif_bf
    m
)
