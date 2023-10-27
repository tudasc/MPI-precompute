
set_property(TEST PROPERTY TIMEOUT 60)

add_test(NAME test01_multi_TU
        COMMAND ${CMAKE_SOURCE_DIR}/tests/run_test_multi_TU.sh ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/01
)

add_test(NAME test02_multi_TU
        COMMAND ${CMAKE_SOURCE_DIR}/tests/run_test_multi_TU.sh ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/02
)


set_tests_properties(test01_multi_TU PROPERTIES PROCESSORS 2)
set_tests_properties(test02_multi_TU PROPERTIES PROCESSORS 2)
#set_tests_properties(test00 PROPERTIES DISABLED true)


