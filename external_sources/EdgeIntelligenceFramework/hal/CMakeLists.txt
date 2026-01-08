# Hardware Abstraction Layer (HAL)

add_library(eif_hal STATIC
    platforms/generic/eif_hal_generic.c
    platforms/generic/eif_hal_impl_mock.c
    src/generic/eif_hal_compute_generic.c
    src/generic/eif_hal_mock_generic.c
)

target_include_directories(eif_hal PUBLIC
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
)

# Enable POSIX implementation for Linux/macOS builds
if(UNIX)
    target_compile_definitions(eif_hal PRIVATE EIF_HAL_PLATFORM_POSIX)
endif()

# Link with m for math functions
target_link_libraries(eif_hal PUBLIC m)

# Optional: ARM Cortex-M support (add to build manually for embedded targets)
# For cross-compilation, add: platforms/arm_cm/eif_hal_arm_cm.c
