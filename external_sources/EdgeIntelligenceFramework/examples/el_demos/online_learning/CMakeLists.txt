add_executable(online_learning_demo main.c)

target_include_directories(online_learning_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../el/include
)

target_link_libraries(online_learning_demo
    eif_core
    eif_el
    m
)
