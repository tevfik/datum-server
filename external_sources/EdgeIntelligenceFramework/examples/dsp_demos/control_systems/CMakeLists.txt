add_executable(control_systems_demo main.c)

target_include_directories(control_systems_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../dsp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
)

target_link_libraries(control_systems_demo PRIVATE eif_dsp eif_core m)
