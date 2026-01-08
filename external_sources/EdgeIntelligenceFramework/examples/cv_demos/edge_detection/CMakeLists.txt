# Edge Detection Demo
add_executable(edge_detection_demo main.c)
target_link_libraries(edge_detection_demo PRIVATE eif_cv eif_core m)
target_include_directories(edge_detection_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../computer-vision/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
)
