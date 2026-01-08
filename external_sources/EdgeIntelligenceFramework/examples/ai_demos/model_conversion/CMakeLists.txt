add_executable(model_conversion_demo main.c)
target_include_directories(model_conversion_demo PRIVATE 
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/neural-engine/include
    ${CMAKE_SOURCE_DIR}/examples/common
)
target_link_libraries(model_conversion_demo eif_core eif_dl m)
set_target_properties(model_conversion_demo PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
