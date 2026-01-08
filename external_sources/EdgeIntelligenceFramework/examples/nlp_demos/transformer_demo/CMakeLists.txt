add_executable(transformer_demo main.c)

target_include_directories(transformer_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../nlp/include
)

target_link_libraries(transformer_demo
    eif_core
    eif_nlp
    m
)
