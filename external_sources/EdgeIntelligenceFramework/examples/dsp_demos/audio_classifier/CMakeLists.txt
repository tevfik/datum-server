add_executable(audio_classifier_demo main.c)
target_include_directories(audio_classifier_demo PRIVATE 
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/signal-processing/include
    ${CMAKE_SOURCE_DIR}/neural-engine/include
    ${CMAKE_SOURCE_DIR}/examples/common
)
target_link_libraries(audio_classifier_demo eif_core eif_dsp eif_dl m)
set_target_properties(audio_classifier_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
