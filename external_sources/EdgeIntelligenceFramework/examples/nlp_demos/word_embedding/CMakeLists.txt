add_executable(embedding_demo main.c)

target_include_directories(embedding_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../nlp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
)

target_link_libraries(embedding_demo PRIVATE
    eif_nlp
    eif_core
    m
)
