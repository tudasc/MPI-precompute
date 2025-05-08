

# LULESH
add_test(NAME LULESH
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_mini_app.sh ${USECASE_SANITIZER_BINARY_DIR}/setup_env.sh ${CMAKE_CURRENT_SOURCE_DIR}/lulesh/lulesh.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
# mini app test take longer
set_tests_properties(LULESH PROPERTIES TIMEOUT 500)


# HPCCG
add_test(NAME HPCCG
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_mini_app.sh ${USECASE_SANITIZER_BINARY_DIR}/setup_env.sh ${CMAKE_CURRENT_SOURCE_DIR}/hpccg/hpccg.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
# mini app test take longer
set_tests_properties(HPCCG PROPERTIES TIMEOUT 500)

# TeaLeaf
add_test(NAME TEALEAF
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_mini_app.sh ${USECASE_SANITIZER_BINARY_DIR}/setup_env.sh ${CMAKE_CURRENT_SOURCE_DIR}/teaLeaf/teaLeaf.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
# mini app test take longer
set_tests_properties(TEALEAF PROPERTIES TIMEOUT 800)