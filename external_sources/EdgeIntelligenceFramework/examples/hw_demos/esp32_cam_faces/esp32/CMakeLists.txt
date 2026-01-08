# ESP-IDF CMakeLists.txt for ESP32-CAM Face Detection
# Place this in the esp32 subdirectory when deploying

cmake_minimum_required(VERSION 3.16)

# Include ESP-IDF components
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp32_cam_faces)

# Component directories
set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_CURRENT_SOURCE_DIR}/../../.."  # EIF library
)
