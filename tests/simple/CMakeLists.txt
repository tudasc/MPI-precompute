
set_property(TEST PROPERTY TIMEOUT 60)

add_test(NAME test01
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/01
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test02
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/02-multi_func
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test03
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/03
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test04
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/04
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test05
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/05
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test06
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/06
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test07
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/07
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test08
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/08
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test09
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/09
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test10
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/10
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test11
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/11
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

add_test(NAME test12
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/12
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

set_tests_properties(test10 PROPERTIES DISABLED true)
set_tests_properties(test11 PROPERTIES DISABLED true)
#set_tests_properties(test00 PROPERTIES DISABLED true)

