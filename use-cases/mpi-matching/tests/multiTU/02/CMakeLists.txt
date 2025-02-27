cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0118 NEW)

project("Testcase 2")

find_package(MPI REQUIRED)

add_executable(test
        comm.c
        main.c)

target_link_libraries(test
        PRIVATE
        MPI::MPI_C)