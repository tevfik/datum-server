# EIF Computer Vision Library

add_library(eif_cv STATIC
    src/eif_cv_image.c
    src/eif_cv_filter.c
    src/eif_cv_feature.c
    src/eif_cv_morph.c
    src/eif_cv_detect.c
    src/eif_cv_track.c
    src/eif_cv_haar.c
)

target_include_directories(eif_cv PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../hal/include
)

target_link_libraries(eif_cv PUBLIC eif_core m)

# Tests
option(EIF_CV_BUILD_TESTS "Build Computer Vision tests" ON)

if(EIF_CV_BUILD_TESTS)
    add_executable(cv_tests
        tests/test_cv.c
    )
    
    target_link_libraries(cv_tests PRIVATE eif_cv eif_core m)
    
    target_include_directories(cv_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    )

    add_test(NAME test_cv COMMAND cv_tests)

    # Coverage tests
    add_executable(cv_coverage_tests
        tests/test_cv_coverage.c
    )
    
    target_link_libraries(cv_coverage_tests PRIVATE eif_cv eif_core m)
    
    target_include_directories(cv_coverage_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    )

    add_test(NAME test_cv_coverage COMMAND cv_coverage_tests)

    # Filter Coverage tests
    add_executable(cv_filter_coverage_tests
        tests/test_cv_filter_coverage.c
    )
    
    target_link_libraries(cv_filter_coverage_tests PRIVATE eif_cv eif_core m)
    
    target_include_directories(cv_filter_coverage_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
    )

    add_test(NAME test_cv_filter_coverage COMMAND cv_filter_coverage_tests)
endif()
