cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0118 NEW)

project("MPI Assertion Checking")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif (NOT CMAKE_BUILD_TYPE)
message("Build Type: ${CMAKE_BUILD_TYPE}")


find_package(LLVM 16 REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(openmpi-patch)
#add the mpi requirement
add_subdirectory(WholeProgramDevirtAnalysisPass)
add_subdirectory(mpi_compiler_assistance_matching_pass)


ExternalProject_Get_property(ompi INSTALL_DIR)

set(PASS_LIB ${CMAKE_BINARY_DIR}/mpi_compiler_assistance_matching_pass/libmpi_compiler_assistance_matching_pass.so)

add_custom_target(setup_env.sh ALL
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/setup_env.sh.in ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export PATH=\\$$PATH:${INSTALL_DIR}/bin" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export MPICC=${INSTALL_DIR}/bin/mpicc" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export MPICXX=${INSTALL_DIR}/bin/mpicxx" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export CPATH=\\$$CPATH:${INSTALL_DIR}/include" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LIBRARY_PATH=\\$$LIBRARY_PATH:${INSTALL_DIR}/lib" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LD_LIBRARY_PATH=\\$$LD_LIBRARY_PATH:${INSTALL_DIR}/lib" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export MPI_COMPILER_ASSISTANCE_PASS=${PASS_LIB}" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export WHOLE_PROGRAM_DEVIRT_ANALYSIS_PASS=${CMAKE_BINARY_DIR}/WholeProgramDevirtAnalysisPass/libwholeProgramDevirtAnalysisPass.so" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh

        DEPENDS setup_env.sh.in)

add_custom_target(run.sh ALL
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/run.sh ${CMAKE_CURRENT_BINARY_DIR}/run.sh

        DEPENDS run.sh)

enable_testing()
add_subdirectory(tests)
