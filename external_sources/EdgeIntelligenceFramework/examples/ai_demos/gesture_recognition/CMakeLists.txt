add_executable(gesture_recognition_demo main.c)
target_include_directories(gesture_recognition_demo PRIVATE 
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/signal-processing/include
    ${CMAKE_SOURCE_DIR}/examples/common
)
target_link_libraries(gesture_recognition_demo eif_core eif_dsp m)
set_target_properties(gesture_recognition_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
