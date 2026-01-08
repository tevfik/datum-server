add_executable(wake_word_demo main.c)

target_include_directories(wake_word_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/ml/include
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/dsp/include
)

target_link_libraries(wake_word_demo PRIVATE m)
