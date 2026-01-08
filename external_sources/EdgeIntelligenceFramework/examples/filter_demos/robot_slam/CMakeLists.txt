add_executable(robot_slam_demo main.c)
target_include_directories(robot_slam_demo PRIVATE 
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/bayesian-filters/include
    ${CMAKE_SOURCE_DIR}/examples/common
)
target_link_libraries(robot_slam_demo eif_core eif_bf m)
set_target_properties(robot_slam_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
