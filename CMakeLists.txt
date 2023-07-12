cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0118 NEW)

project("MPI Assertion Checking")

find_package(LLVM REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(openmpi-patch)
#add the mpi requirement
add_subdirectory(mpi_compiler_assistance_matching_pass)

#TODO add a setup_env target
ExternalProject_Get_property(ompi INSTALL_DIR)

add_custom_target(setup_env.sh
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/setup_env.sh.in ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export PATH=\\$$PATH:${INSTALL_DIR}/bin" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export CPATH=\\$$CPATH:${INSTALL_DIR}/include" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LIBRARY_PATH=\\$$LIBRARY_PATH:${INSTALL_DIR}/lib" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LD_LIBRARY_PATH=\\$$LD_LIBRARY_PATH:${INSTALL_DIR}/lib" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh

        DEPENDS setup_env.sh.in)

#TODO add test target
