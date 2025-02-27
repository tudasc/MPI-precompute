cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0118 NEW)

project("Precompute")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE DEBUG)
endif (NOT CMAKE_BUILD_TYPE)
message("Build Type: ${CMAKE_BUILD_TYPE}")


find_package(LLVM 16 REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(precompute_runtime_lib)

set(PASS_LIB ${CMAKE_BINARY_DIR}/mpi_compiler_assistance_matching_pass/libmpi_compiler_assistance_matching_pass.so)
set(PRECOMPUTE_LIB_PATH ${CMAKE_BINARY_DIR}/precompute_lib)

enable_testing()

option(MPI_USE_CASE "Build the mpi usecase (precomputation of MPI message envelopes)" ON)

if (MPI_USE_CASE)
    add_subdirectory(use-cases/mpi-matching)
endif ()
