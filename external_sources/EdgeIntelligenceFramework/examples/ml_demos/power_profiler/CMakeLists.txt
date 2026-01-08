add_executable(power_demo main.c)

target_include_directories(power_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
)

target_link_libraries(power_demo
    eif_core
    m
)
