cmake_minimum_required(VERSION 3.10)

add_executable(pid_sim_demo main.c)

target_link_libraries(pid_sim_demo PRIVATE eif_core eif_dsp m)
