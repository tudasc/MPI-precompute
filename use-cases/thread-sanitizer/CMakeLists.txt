
set(USECASE_SANITIZER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(sanitizer_precompute_compiler_pass)
#add_subdirectory(tests)

add_custom_target(run.sh-sanitizer ALL
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/run.sh ${CMAKE_CURRENT_BINARY_DIR}/run.sh
        DEPENDS run.sh)

