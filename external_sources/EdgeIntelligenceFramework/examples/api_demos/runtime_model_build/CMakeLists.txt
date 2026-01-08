add_executable(runtime_model_demo main.c)

target_include_directories(runtime_model_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/ml/include
    ${CMAKE_SOURCE_DIR}/core/include
)

target_link_libraries(runtime_model_demo PRIVATE m)
