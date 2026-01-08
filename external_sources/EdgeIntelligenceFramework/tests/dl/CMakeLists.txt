# DL Context Tests

add_executable(test_dl_context test_dl_context.c)
target_link_libraries(test_dl_context eif_dl m)
target_include_directories(test_dl_context PRIVATE 
    ${CMAKE_SOURCE_DIR}/dl/include
    ${CMAKE_SOURCE_DIR}/core/include
)

add_test(NAME dl_context_tests COMMAND test_dl_context)

add_executable(test_custom_op test_custom_op.c)
target_link_libraries(test_custom_op eif_dl m)
target_include_directories(test_custom_op PRIVATE 
    ${CMAKE_SOURCE_DIR}/dl/include
    ${CMAKE_SOURCE_DIR}/core/include
)
add_test(NAME custom_op_test COMMAND test_custom_op)

add_executable(test_dl_core_coverage test_dl_core_coverage.c)
target_compile_definitions(test_dl_core_coverage PRIVATE EIF_STANDALONE_TEST)
target_link_libraries(test_dl_core_coverage eif_dl m)
target_include_directories(test_dl_core_coverage PRIVATE 
    ${CMAKE_SOURCE_DIR}/dl/include
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/tests/framework
)
add_test(NAME dl_core_coverage_tests COMMAND test_dl_core_coverage)

add_executable(test_depthwise_int8 test_depthwise_int8.c)
target_link_libraries(test_depthwise_int8 eif_dl m)
target_include_directories(test_depthwise_int8 PRIVATE 
    ${CMAKE_SOURCE_DIR}/dl/include
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/tests/framework
)
add_test(NAME depthwise_int8_test COMMAND test_depthwise_int8)

add_executable(test_argmax test_argmax.c)
target_compile_definitions(test_argmax PRIVATE EIF_STANDALONE_TEST)
target_link_libraries(test_argmax eif_dl m)
target_include_directories(test_argmax PRIVATE 
    ${CMAKE_SOURCE_DIR}/dl/include
    ${CMAKE_SOURCE_DIR}/dl/src
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/tests/framework
)
add_test(NAME argmax_test COMMAND test_argmax)
