add_executable(gesture_demo main.c)

target_include_directories(gesture_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/ml/include
    ${CMAKE_SOURCE_DIR}/core/include
)

target_link_libraries(gesture_demo PRIVATE m)
