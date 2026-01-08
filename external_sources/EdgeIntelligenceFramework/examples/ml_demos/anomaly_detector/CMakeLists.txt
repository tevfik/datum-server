add_executable(anomaly_detector_demo main.c)
target_include_directories(anomaly_detector_demo PRIVATE 
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/data-analysis/include
    ${CMAKE_SOURCE_DIR}/examples/common
)
target_link_libraries(anomaly_detector_demo eif_core eif_da m)
set_target_properties(anomaly_detector_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
