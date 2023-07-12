
add_test(NAME test01
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/01
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(test01 PROPERTIES TIMEOUT 60)

add_test(NAME test02
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test.sh ${CMAKE_CURRENT_SOURCE_DIR}/02-multi_func
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
set_tests_properties(test02 PROPERTIES TIMEOUT 60)