# Feature Matching Demo
add_executable(feature_matching_demo main.c)
target_link_libraries(feature_matching_demo PRIVATE eif_cv eif_core m)
target_include_directories(feature_matching_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../computer-vision/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
)
