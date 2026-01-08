add_executable(predictive_maintenance_demo main.c)

target_include_directories(predictive_maintenance_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../ml/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
)

target_link_libraries(predictive_maintenance_demo PRIVATE eif_ml eif_core m)
