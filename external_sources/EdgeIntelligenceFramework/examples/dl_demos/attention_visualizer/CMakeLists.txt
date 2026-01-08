add_executable(attention_visualizer main.c)

target_include_directories(attention_visualizer PRIVATE
    ${CMAKE_SOURCE_DIR}/ml/include
    ${CMAKE_SOURCE_DIR}/core/include
)

target_link_libraries(attention_visualizer PRIVATE m)
