add_executable(smart_activity_demo main.c)

target_include_directories(smart_activity_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/dsp/include
    ${CMAKE_SOURCE_DIR}/ml/include
    ${CMAKE_SOURCE_DIR}/examples/common
)

target_link_libraries(smart_activity_demo PRIVATE m)
