
set_property(TEST PROPERTY TIMEOUT 60)

add_test(NAME test01
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/01
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test02
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/02-multi_func
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test03
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/03
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test04
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/04
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test05
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/05
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test06
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/06
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test07
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/07
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test08
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/08
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test09
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/09
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test10
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/10
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test11
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/11
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test12
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/12
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test13
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/13
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test14
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/14
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test15
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/15
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test16
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/16
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test17
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/17
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test18
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/18
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test19
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/19
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_test(NAME test20
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/20
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test21
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/21
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test22
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/22
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test23
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${CMAKE_CURRENT_SOURCE_DIR}/23
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

set_tests_properties(test01 PROPERTIES PROCESSORS 2)
set_tests_properties(test02 PROPERTIES PROCESSORS 2)
set_tests_properties(test03 PROPERTIES PROCESSORS 2)
set_tests_properties(test04 PROPERTIES PROCESSORS 2)
set_tests_properties(test05 PROPERTIES PROCESSORS 2)
set_tests_properties(test06 PROPERTIES PROCESSORS 2)
set_tests_properties(test07 PROPERTIES PROCESSORS 2)
set_tests_properties(test08 PROPERTIES PROCESSORS 2)
set_tests_properties(test09 PROPERTIES PROCESSORS 2)
set_tests_properties(test10 PROPERTIES PROCESSORS 2)
set_tests_properties(test11 PROPERTIES PROCESSORS 2)
set_tests_properties(test12 PROPERTIES PROCESSORS 2)
set_tests_properties(test13 PROPERTIES PROCESSORS 2)
set_tests_properties(test14 PROPERTIES PROCESSORS 2)
set_tests_properties(test15 PROPERTIES PROCESSORS 2)
set_tests_properties(test16 PROPERTIES PROCESSORS 2)
set_tests_properties(test17 PROPERTIES PROCESSORS 2)
set_tests_properties(test18 PROPERTIES PROCESSORS 2)
set_tests_properties(test19 PROPERTIES PROCESSORS 2)
set_tests_properties(test20 PROPERTIES PROCESSORS 2)
set_tests_properties(test21 PROPERTIES PROCESSORS 2)
set_tests_properties(test22 PROPERTIES PROCESSORS 2)
set_tests_properties(test23 PROPERTIES PROCESSORS 2)


# currently not implemented feature: other comm than comm world
set_tests_properties(test04 PROPERTIES DISABLED true)
# currently not implemented feature: reading input data (C-Style)
set_tests_properties(test10 PROPERTIES DISABLED true)
# currently not implemented feature: reading input data (C-Style)
set_tests_properties(test11 PROPERTIES DISABLED true)
#set_tests_properties(test00 PROPERTIES DISABLED true)

# These testcases Re-Use an envelope and are therefore expected to fail
#TODO better test infrastructure to also account for expected to fail tests
set_tests_properties(test19 PROPERTIES DISABLED true)
set_tests_properties(test20 PROPERTIES DISABLED true)
