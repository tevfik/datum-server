cmake_minimum_required(VERSION 3.10)
# Centralized project version
set(PROJECT_VERSION "1.0.0")
project(EdgeIntelligenceFramework VERSION ${PROJECT_VERSION} LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Generate version header from single source of truth
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/core/include/eif_version.h.in
			   ${CMAKE_CURRENT_SOURCE_DIR}/core/include/eif_version.h @ONLY)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Enable testing globally
enable_testing()

# Code Coverage Configuration
option(ENABLE_COVERAGE "Enable code coverage generation" OFF)
if(ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Enabling code coverage")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    else()
        message(WARNING "Code coverage not supported for this compiler")
    endif()
endif()

# OpenMP Support
find_package(OpenMP)
if(OpenMP_C_FOUND)
    message(STATUS "OpenMP Found. Enabling multi-threading support.")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_C_FLAGS}")
endif()

# Core libraries
add_subdirectory(core)
add_subdirectory(dl)
add_subdirectory(dsp)
add_subdirectory(bf)
add_subdirectory(da)
add_subdirectory(ml)
add_subdirectory(nlp)
add_subdirectory(el)
add_subdirectory(cv)
add_subdirectory(hal)


# Examples (API tests + Demos)
add_subdirectory(examples)

# Tests and Benchmarks
add_subdirectory(tests)
add_subdirectory(benchmarks)
