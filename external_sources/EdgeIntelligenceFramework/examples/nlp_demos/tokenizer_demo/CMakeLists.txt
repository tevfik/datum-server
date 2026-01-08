add_executable(nlp_demo main.c)

target_include_directories(nlp_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../nlp/include
)

target_link_libraries(nlp_demo 
    eif_core
    eif_nlp
    m
)
