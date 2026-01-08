# api_tests CMakeLists.txt

add_subdirectory(dsp_test)
add_subdirectory(dsp_fixed_test)
add_subdirectory(matrix_test)
add_subdirectory(kalman_test)
add_subdirectory(kalman_fixed_test)
add_subdirectory(ekf_test)
add_subdirectory(generic_test)

# Optional tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/matrix_fixed_test/CMakeLists.txt)
    add_subdirectory(matrix_fixed_test)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fixedpoint_test/CMakeLists.txt)
    add_subdirectory(fixedpoint_test)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/hello_world/CMakeLists.txt)
    add_subdirectory(hello_world)
endif()

