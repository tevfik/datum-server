add_executable(model_evaluation_demo main.c)

target_include_directories(model_evaluation_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/ml/include
    ${CMAKE_SOURCE_DIR}/core/include
)

target_link_libraries(model_evaluation_demo PRIVATE m)
