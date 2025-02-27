
set_property(TEST PROPERTY TIMEOUT 60)

add_test(NAME test01
        COMMAND ../run_test_single_TU.sh 01
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test02
        COMMAND ../run_test_single_TU.sh 02-multi_func
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test03
        COMMAND ../run_test_single_TU.sh 03
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test04
        COMMAND ../run_test_single_TU.sh 04
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test05
        COMMAND ../run_test_single_TU.sh 05
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test06
        COMMAND ../run_test_single_TU.sh 06
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test07
        COMMAND ../run_test_single_TU.sh 07
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test08
        COMMAND ../run_test_single_TU.sh 08
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test09
        COMMAND ../run_test_single_TU.sh 09
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test10
        COMMAND ../run_test_single_TU.sh 10
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test11
        COMMAND ../run_test_single_TU.sh 11
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test12
        COMMAND ../run_test_single_TU.sh 12
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test13
        COMMAND ../run_test_single_TU.sh 13
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test14
        COMMAND ../run_test_single_TU.sh 14
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test15
        COMMAND ../run_test_single_TU.sh 15
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test16
        COMMAND ../run_test_single_TU.sh 16
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test17
        COMMAND ../run_test_single_TU.sh 17
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test18
        COMMAND ../run_test_single_TU.sh 18
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test19
        COMMAND ../run_test_single_TU.sh 19
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_test(NAME test20
        COMMAND ../run_test_single_TU.sh 20
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test21
        COMMAND ../run_test_single_TU.sh 21
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test22
        COMMAND ../run_test_single_TU.sh 22
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test23
        COMMAND ../run_test_single_TU.sh 23
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test24
        COMMAND ../run_test_single_TU.sh 24
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test25
        COMMAND ../run_test_single_TU.sh 25
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test26
        COMMAND ../run_test_single_TU.sh 26
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME test27
        COMMAND ../run_test_single_TU.sh 27
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME cmake-c
        COMMAND ../run_test_single_TU.sh cmake-c
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME cmake-cpp
        COMMAND ../run_test_single_TU.sh cmake-cpp
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
set_tests_properties(test24 PROPERTIES PROCESSORS 2)
set_tests_properties(test25 PROPERTIES PROCESSORS 2)
set_tests_properties(test26 PROPERTIES PROCESSORS 2)
set_tests_properties(test27 PROPERTIES PROCESSORS 2)

# currently not implemented feature: non-contiguous dtypes
set_tests_properties(test03 PROPERTIES DISABLED true)
# currently not implemented feature: other comm than comm world
set_tests_properties(test04 PROPERTIES DISABLED true)
# currently not implemented feature: reading input data (C-Style)
set_tests_properties(test10 PROPERTIES DISABLED true)
# currently not implemented feature: reading input data (C-Style)
set_tests_properties(test11 PROPERTIES DISABLED true)
# currently not implemented: use uf shared ptr
set_tests_properties(test27 PROPERTIES DISABLED true)
#set_tests_properties(test00 PROPERTIES DISABLED true)


# These testcases Re-Use an envelope and are therefore expected to NOT use MPIOPT
# the pass should still produce a running executable though, where precompute finds out that MPIOPT is not applicable
set_tests_properties(test19 PROPERTIES ENVIRONMENT EXPECT_NO_MPIOPT=1)
set_tests_properties(test20 PROPERTIES ENVIRONMENT EXPECT_NO_MPIOPT=1)
set_tests_properties(cmake-c PROPERTIES ENVIRONMENT EXPECT_NO_MPIOPT=1)
set_tests_properties(cmake-cpp PROPERTIES ENVIRONMENT EXPECT_NO_MPIOPT=1)

set_tests_properties(test26 PROPERTIES ENVIRONMENT ARGS_TO_TEST_EXEC=-t1\ 2\ -t2\ 2)

