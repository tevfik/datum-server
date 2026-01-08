add_executable(kmeans_demo main.c)

target_include_directories(kmeans_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../ml/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
)

target_link_libraries(kmeans_demo PRIVATE
    eif_ml
    eif_core
    m
)
