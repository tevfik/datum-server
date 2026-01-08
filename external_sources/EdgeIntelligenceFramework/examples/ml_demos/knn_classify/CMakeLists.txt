cmake_minimum_required(VERSION 3.10)

add_executable(knn_classify_demo main.c)

target_link_libraries(knn_classify_demo PRIVATE eif_core eif_ml m)
