add_executable(rl_gridworld_demo main.c)
target_include_directories(rl_gridworld_demo PRIVATE 
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/el/include
    ${CMAKE_SOURCE_DIR}/examples/common
)
target_link_libraries(rl_gridworld_demo eif_core eif_el m)
set_target_properties(rl_gridworld_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

