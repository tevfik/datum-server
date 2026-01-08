message(STATUS "Configuring MNIST CNN example")
add_executable(mnist_cnn main.c)

target_include_directories(mnist_cnn PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_SOURCE_DIR}/neural-engine/include
    ${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_SOURCE_DIR}/core/include
)

target_link_libraries(mnist_cnn PRIVATE
    eif_dl
    eif_core
    m
)
