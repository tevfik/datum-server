add_executable(edge_intelligence_demo main.c)

target_include_directories(edge_intelligence_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../dl/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../ml/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
)

target_link_libraries(edge_intelligence_demo PRIVATE eif_ml eif_dl eif_core m)
