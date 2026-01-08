# IMU Fusion Demo CMakeLists.txt

add_executable(imu_fusion_demo main.c)

target_link_libraries(imu_fusion_demo PRIVATE
    eif_bf
    eif_core
    m
)
