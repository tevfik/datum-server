add_executable(sequence_classifier main.c)

target_include_directories(sequence_classifier PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ml/include
    ${CMAKE_SOURCE_DIR}/core/include
)

target_link_libraries(sequence_classifier PRIVATE m)
