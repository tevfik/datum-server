add_executable(activity_recognition_demo main.c)

target_include_directories(activity_recognition_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../ml/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
)

target_link_libraries(activity_recognition_demo PRIVATE eif_ml eif_core m)
