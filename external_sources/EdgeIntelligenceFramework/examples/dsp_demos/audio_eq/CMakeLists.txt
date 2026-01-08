cmake_minimum_required(VERSION 3.10)

add_executable(audio_eq_demo main.c)

target_link_libraries(audio_eq_demo PRIVATE eif_core eif_dsp m)
