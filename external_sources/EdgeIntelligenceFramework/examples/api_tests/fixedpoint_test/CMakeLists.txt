add_executable(fixedpoint_test main.c)

target_include_directories(fixedpoint_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(fixedpoint_test PRIVATE
    eif_core
)
