add_executable(kws_demo main.c)

target_include_directories(kws_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/neural-engine/include
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/signal-processing/include
    ${CMAKE_SOURCE_DIR}/examples/common
)

target_link_libraries(kws_demo PRIVATE
    eif_dl
    eif_dsp
    eif_core
    m
)
