add_executable(signal_analysis_demo main.c)
target_include_directories(signal_analysis_demo PRIVATE 
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/signal-processing/include
    ${CMAKE_SOURCE_DIR}/examples/common
)
target_link_libraries(signal_analysis_demo eif_core eif_dsp m)
set_target_properties(signal_analysis_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
