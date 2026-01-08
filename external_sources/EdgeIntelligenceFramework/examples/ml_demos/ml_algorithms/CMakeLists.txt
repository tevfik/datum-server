add_executable(ml_algorithms_demo main.c)

target_include_directories(ml_algorithms_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/data-analysis/include
    ${CMAKE_SOURCE_DIR}/examples/common
)

target_link_libraries(ml_algorithms_demo PRIVATE
    eif_da
    eif_core
    m
)
