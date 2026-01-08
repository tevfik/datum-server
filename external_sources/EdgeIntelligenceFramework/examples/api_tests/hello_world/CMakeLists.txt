add_executable(hello_world main.c)

target_link_libraries(hello_world PRIVATE
    eif_core
    eif_dl
    eif_dsp
    eif_bf
    eif_da
)
