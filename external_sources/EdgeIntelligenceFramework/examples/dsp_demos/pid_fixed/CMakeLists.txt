cmake_minimum_required(VERSION 3.10)

# Add fix16pid sources
set(FIX16_SOURCES
    ../../common/external/fix16pid/fix16.c
    ../../common/external/fix16pid/fix16pid.c
)

add_executable(pid_fixed_demo 
    main.c
    ../../common/eif_demo_cli.c
    ${FIX16_SOURCES}
)

target_include_directories(pid_fixed_demo PRIVATE 
    ../../common
    ../../common/external/fix16pid
)

# Link math library (still needed for main simulation which uses float for "Plant")
target_link_libraries(pid_fixed_demo PRIVATE m)
