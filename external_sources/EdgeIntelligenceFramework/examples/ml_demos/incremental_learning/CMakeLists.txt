add_executable(incremental_learning_demo main.c)

target_include_directories(incremental_learning_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/ml/include
    ${CMAKE_SOURCE_DIR}/core/include
)

target_link_libraries(incremental_learning_demo PRIVATE m)
