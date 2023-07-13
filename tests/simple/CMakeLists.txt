
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
set_tests_properties(test03 PROPERTIES DISABLED true)

add_test(NAME test04
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/04
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
set_tests_properties(test04 PROPERTIES DISABLED true)

add_test(NAME test05
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/05
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
set_tests_properties(test05 PROPERTIES DISABLED true)

add_test(NAME test06
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/06
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
set_tests_properties(test06 PROPERTIES DISABLED true)

