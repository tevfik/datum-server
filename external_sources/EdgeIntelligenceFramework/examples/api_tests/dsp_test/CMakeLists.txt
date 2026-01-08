add_executable(dsp_test main.c)

target_include_directories(dsp_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(dsp_test PRIVATE
    eif_core
    eif_dsp
    m
)
