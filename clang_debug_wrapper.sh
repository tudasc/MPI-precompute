#!/bin/bash

# wrapper to debug clang invocations with more complicated build systems

CLANG_WRAPPER=/work/home/tj75qeje/mpi-comp-match/build/clang_wrap_cxx

# echo "$CLANG_WRAPPER $@" >> /home/tj75qeje/mpi-comp-match/build/clang_invoke_log

DEBUG_CLANG_WRAPPER=true $CLANG_WRAPPER "$@" >> /home/tj75qeje/mpi-comp-match/build/clang_invoke_log