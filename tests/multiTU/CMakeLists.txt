
set_property(TEST PROPERTY TIMEOUT 60)

add_test(NAME test01_multi_TU
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_multi_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/01
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)


set_tests_properties(test01_multi_TU PROPERTIES PROCESSORS 2)
#set_tests_properties(test00 PROPERTIES DISABLED true)


