# Object Tracking Demo
add_executable(object_tracking_demo main.c)
target_link_libraries(object_tracking_demo PRIVATE eif_cv eif_core m)
target_include_directories(object_tracking_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../computer-vision/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
)
