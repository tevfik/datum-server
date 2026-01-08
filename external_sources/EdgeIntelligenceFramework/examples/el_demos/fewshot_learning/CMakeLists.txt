add_executable(fewshot_learning_demo main.c)

target_include_directories(fewshot_learning_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../el/include
)

target_link_libraries(fewshot_learning_demo
    eif_core
    eif_el
    m
)
