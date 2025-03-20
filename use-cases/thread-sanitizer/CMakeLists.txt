
set(USECASE_SANITIZER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(sanitizer_precompute_compiler_pass)
#add_subdirectory(tests)

add_custom_target(run.sh-sanitizer ALL
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/run.sh ${CMAKE_CURRENT_BINARY_DIR}/run.sh
        DEPENDS run.sh)

add_custom_target(setup_env.sh_sanitizer ALL
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/setup_env.sh.in ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LIBRARY_PATH=${PRECOMPUTE_LIB_PATH}:\\$$LIBRARY_PATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LD_LIBRARY_PATH=${PRECOMPUTE_LIB_PATH}:\\$$LD_LIBRARY_PATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export SANITIZER_PASS=$<TARGET_FILE:sanitizer_precompute_pass>" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh
        DEPENDS setup_env.sh.in)
