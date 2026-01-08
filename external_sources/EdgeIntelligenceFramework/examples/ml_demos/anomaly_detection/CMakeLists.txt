add_executable(anomaly_demo main.c)

target_include_directories(anomaly_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../da/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../ml/include
)

target_link_libraries(anomaly_demo
    eif_core
    eif_da
    eif_ml
    m
)
