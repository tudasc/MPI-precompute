
set(USECASE_SANITIZER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(sanitizer_precompute_compiler_pass)
add_subdirectory(tests)
add_subdirectory(sample_apps)

add_custom_target(run.sh-sanitizer ALL
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/run.sh ${CMAKE_CURRENT_BINARY_DIR}/run.sh
        DEPENDS run.sh)

add_custom_target(clang_wrap_cc-sanitizer ALL
        COMMAND cp ${CMAKE_SOURCE_DIR}/clang_wrapper.sh ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cc && sed -i '3s/.*/compiler=clang/' ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cc
        DEPENDS ${CMAKE_SOURCE_DIR}/clang_wrapper.sh)

add_custom_target(clang_wrap_cxx-sanitizer ALL
        COMMAND cp ${CMAKE_SOURCE_DIR}/clang_wrapper.sh ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cxx && sed -i '3s/.*/compiler=clang++/' ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cxx
        DEPENDS ${CMAKE_SOURCE_DIR}/clang_wrapper.sh)

add_custom_target(flang_wrap-sanitizer ALL
        COMMAND cp ${CMAKE_SOURCE_DIR}/clang_wrapper.sh ${CMAKE_CURRENT_BINARY_DIR}/flang_wrap && sed -i '3s/.*/compiler=flang/' ${CMAKE_CURRENT_BINARY_DIR}/flang_wrap
        DEPENDS ${CMAKE_SOURCE_DIR}/clang_wrapper.sh)

add_custom_target(setup_env.sh_sanitizer ALL
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/setup_env.sh.in ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LIBRARY_PATH=${PRECOMPUTE_LIB_PATH}:\\$$LIBRARY_PATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LD_LIBRARY_PATH=${PRECOMPUTE_LIB_PATH}:\\$$LD_LIBRARY_PATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export SANITIZER_PASS=$<TARGET_FILE:sanitizer_precompute_pass>" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export COMPILER_PASS=$<TARGET_FILE:sanitizer_precompute_pass>" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export USE_COMPILER_PASS=true" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export CLANG_WRAP_CC=${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cc" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export CLANG_WRAP_CXX=${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cxx" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh
        echo "export CLANG_WRAP_FC=${CMAKE_CURRENT_BINARY_DIR}/flang_wrap" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh
        DEPENDS setup_env.sh.in)


