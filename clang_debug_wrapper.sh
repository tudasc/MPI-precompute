#!/bin/bash

# wrapper to debug clang invocations with more complicated build systems

CLANG_WRAPPER=/work/home/tj75qeje/mpi-comp-match/clang_wrapper.sh

echo "$CLANG_WRAPPER $@" >> /home/tj75qeje/mpi-comp-match/build/clang_invoke_log

# cp "${@: -1}" /home/tj75qeje/mpi-comp-match/build/test_mpi.cpp

$CLANG_WRAPPER "$@" >> /home/tj75qeje/mpi-comp-match/build/clang_invoke_log