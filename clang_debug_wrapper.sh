#!/bin/bash

# wrapper to debug clang invocations with more complicated build systems

echo "clang++ $@" >> /home/tj75qeje/mpi-comp-match/build/clang_invoke_log

cp "${@: -1}" /home/tj75qeje/mpi-comp-match/build/test_mpi.cpp

clang++ "$@"