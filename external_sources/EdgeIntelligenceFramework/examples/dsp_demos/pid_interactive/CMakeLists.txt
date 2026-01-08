cmake_minimum_required(VERSION 3.10)

add_executable(pid_interactive_demo 
    main.c
    ../../common/eif_demo_cli.c
)

target_include_directories(pid_interactive_demo PRIVATE 
    ../../common
)

target_link_libraries(pid_interactive_demo PRIVATE eif_core eif_dsp m)
