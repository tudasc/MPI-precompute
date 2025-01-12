cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0118 NEW)

project("MPI Assertion Checking")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE DEBUG)
endif (NOT CMAKE_BUILD_TYPE)
message("Build Type: ${CMAKE_BUILD_TYPE}")


find_package(LLVM 16 REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(precompute_lib)

add_subdirectory(openmpi-patch)
#add the mpi requirement

add_subdirectory(mpi_compiler_assistance_matching_pass)


ExternalProject_Get_property(ompi INSTALL_DIR)

set(PASS_LIB ${CMAKE_BINARY_DIR}/mpi_compiler_assistance_matching_pass/libmpi_compiler_assistance_matching_pass.so)
set(PRECOMPUTE_LIB_PATH ${CMAKE_BINARY_DIR}/precompute_lib)

add_custom_target(setup_env.sh ALL
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/setup_env.sh.in ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export PATH=${INSTALL_DIR}/bin:\\$$PATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export MPICC=${INSTALL_DIR}/bin/mpicc" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export MPICXX=${INSTALL_DIR}/bin/mpicxx" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export CPATH=${INSTALL_DIR}/include:\\$$CPATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LIBRARY_PATH=${INSTALL_DIR}/lib:${PRECOMPUTE_LIB_PATH}:\\$$LIBRARY_PATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LD_LIBRARY_PATH=${INSTALL_DIR}/lib:${PRECOMPUTE_LIB_PATH}:\\$$LD_LIBRARY_PATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export MPI_COMPILER_ASSISTANCE_PASS=${PASS_LIB}" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export WHOLE_PROGRAM_DEVIRT_ANALYSIS_PASS=${CMAKE_BINARY_DIR}/WholeProgramDevirtAnalysisPass/libwholeProgramDevirtAnalysisPass.so" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export OMPI_CC=${CMAKE_BINARY_DIR}/clang_wrap_cc" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export OMPI_CXX=${CMAKE_BINARY_DIR}/clang_wrap_cxx" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export CLANG_WRAP_CC=${CMAKE_BINARY_DIR}/clang_wrap_cc" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export CLANG_WRAP_CXX=${CMAKE_BINARY_DIR}/clang_wrap_cxx" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh

        DEPENDS setup_env.sh.in)

add_custom_target(run.sh ALL
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/run.sh ${CMAKE_CURRENT_BINARY_DIR}/run.sh

        DEPENDS run.sh)

add_custom_target(clang_wrap_cc ALL
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/clang_wrapper.sh ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cc && sed -i '3s/.*/compiler=clang/' ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cc
        DEPENDS clang_wrapper.sh)

add_custom_target(clang_wrap_cxx ALL
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/clang_wrapper.sh ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cxx && sed -i '3s/.*/compiler=clang++/' ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cxx
        DEPENDS clang_wrapper.sh)

enable_testing()
add_subdirectory(tests)
