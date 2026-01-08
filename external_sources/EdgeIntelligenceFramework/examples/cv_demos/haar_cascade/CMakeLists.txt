add_executable(haar_demo main.c)

target_include_directories(haar_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../cv/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../core/include
)

target_link_libraries(haar_demo PRIVATE
    eif_cv
    eif_core
    m
)
