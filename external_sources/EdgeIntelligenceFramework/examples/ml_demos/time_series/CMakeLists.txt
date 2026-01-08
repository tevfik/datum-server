add_executable(time_series_demo main.c)
target_include_directories(time_series_demo PRIVATE 
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/data-analysis/include
    ${CMAKE_SOURCE_DIR}/examples/common
)
target_link_libraries(time_series_demo eif_core eif_da m)
set_target_properties(time_series_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
