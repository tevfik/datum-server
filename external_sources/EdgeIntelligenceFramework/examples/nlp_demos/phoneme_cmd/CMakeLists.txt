add_executable(phoneme_cmd_demo main.c)

target_include_directories(phoneme_cmd_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../nlp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
)

target_link_libraries(phoneme_cmd_demo PRIVATE eif_nlp eif_core m)
