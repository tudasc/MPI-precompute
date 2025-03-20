
set_property(TEST PROPERTY TIMEOUT 60)

function(add_single_tu_testcase TEST_NAME TEST_DIR)
    add_test(NAME ${TEST_NAME}
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_test_single_TU.sh ${USECASE_MPI_BINARY_DIR} ${TEST_DIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    set_tests_properties(${TEST_NAME} PROPERTIES PROCESSORS 2)
endfunction()

add_single_tu_testcase(test01 ${CMAKE_CURRENT_SOURCE_DIR}/01)
add_single_tu_testcase(test02 ${CMAKE_CURRENT_SOURCE_DIR}/02-multi_func)
#add_single_tu_testcase(test03 ${CMAKE_CURRENT_SOURCE_DIR}/03)
#add_single_tu_testcase(test04 ${CMAKE_CURRENT_SOURCE_DIR}/04)
add_single_tu_testcase(test05 ${CMAKE_CURRENT_SOURCE_DIR}/05)
add_single_tu_testcase(test06 ${CMAKE_CURRENT_SOURCE_DIR}/06)
add_single_tu_testcase(test07 ${CMAKE_CURRENT_SOURCE_DIR}/07)
add_single_tu_testcase(test08 ${CMAKE_CURRENT_SOURCE_DIR}/08)
add_single_tu_testcase(test09 ${CMAKE_CURRENT_SOURCE_DIR}/09)
#add_single_tu_testcase(test10 ${CMAKE_CURRENT_SOURCE_DIR}/10)
#add_single_tu_testcase(test11 ${CMAKE_CURRENT_SOURCE_DIR}/11)
add_single_tu_testcase(test12 ${CMAKE_CURRENT_SOURCE_DIR}/12)
add_single_tu_testcase(test13 ${CMAKE_CURRENT_SOURCE_DIR}/13)
add_single_tu_testcase(test14 ${CMAKE_CURRENT_SOURCE_DIR}/14)
add_single_tu_testcase(test15 ${CMAKE_CURRENT_SOURCE_DIR}/15)
add_single_tu_testcase(test16 ${CMAKE_CURRENT_SOURCE_DIR}/16)
add_single_tu_testcase(test17 ${CMAKE_CURRENT_SOURCE_DIR}/17)
add_single_tu_testcase(test18 ${CMAKE_CURRENT_SOURCE_DIR}/18)
add_single_tu_testcase(test19 ${CMAKE_CURRENT_SOURCE_DIR}/19)
add_single_tu_testcase(test20 ${CMAKE_CURRENT_SOURCE_DIR}/20)
add_single_tu_testcase(test21 ${CMAKE_CURRENT_SOURCE_DIR}/21)
add_single_tu_testcase(test22 ${CMAKE_CURRENT_SOURCE_DIR}/22)
add_single_tu_testcase(test23 ${CMAKE_CURRENT_SOURCE_DIR}/23)
add_single_tu_testcase(test24 ${CMAKE_CURRENT_SOURCE_DIR}/24)
add_single_tu_testcase(test25 ${CMAKE_CURRENT_SOURCE_DIR}/25)
add_single_tu_testcase(test26 ${CMAKE_CURRENT_SOURCE_DIR}/26)
#add_single_tu_testcase(test27 ${CMAKE_CURRENT_SOURCE_DIR}/27)

add_single_tu_testcase(cmake-c ${CMAKE_CURRENT_SOURCE_DIR}/cmake-c)
add_single_tu_testcase(cmake-cpp ${CMAKE_CURRENT_SOURCE_DIR}/cmake-cpp)

# currently not implemented feature: non-contiguous dtypes
#set_tests_properties(test03 PROPERTIES DISABLED true)
# currently not implemented feature: other comm than comm world
#set_tests_properties(test04 PROPERTIES DISABLED true)
# currently not implemented feature: reading input data (C-Style)
#set_tests_properties(test10 PROPERTIES DISABLED true)
# currently not implemented feature: reading input data (C-Style)
#set_tests_properties(test11 PROPERTIES DISABLED true)
# currently not implemented: use uf shared ptr
#set_tests_properties(test27 PROPERTIES DISABLED true)
#set_tests_properties(test00 PROPERTIES DISABLED true)


# These testcases Re-Use an envelope and are therefore expected to NOT use MPIOPT
# the pass should still produce a running executable though, where precompute finds out that MPIOPT is not applicable
set_tests_properties(test19 PROPERTIES ENVIRONMENT EXPECT_NO_MPIOPT=1)
set_tests_properties(test20 PROPERTIES ENVIRONMENT EXPECT_NO_MPIOPT=1)
set_tests_properties(cmake-c PROPERTIES ENVIRONMENT EXPECT_NO_MPIOPT=1)
set_tests_properties(cmake-cpp PROPERTIES ENVIRONMENT EXPECT_NO_MPIOPT=1)

set_tests_properties(test26 PROPERTIES ENVIRONMENT ARGS_TO_TEST_EXEC=-t1\ 2\ -t2\ 2)

