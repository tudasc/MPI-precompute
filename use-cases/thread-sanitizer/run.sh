#!/bin/bash


DEFAULT_CFLAGS="-O2 -g -fopenmp -fsanitize=thread ${INCLUDE}"
#LIBS="-lm"
DEFAULT_CXXFLAGS="-std=c++17 -O1 -g -fopenmp -fsanitize=thread ${INCLUDE}"

CFLAGS="${CFLAGS:-$DEFAULT_CFLAGS}"
CXXFLAGS="${CFLAGS:-$DEFAULT_CXXFLAGS}"

#PASS_FLAGS="-flto -fwhole-program-vtables"
PASS_FLAGS="-fuse-ld=lld -flto -fwhole-program-vtables -fno-inline"

# for debugging the pass itself with address sanitizer
export LD_PRELOAD="$(clang -print-file-name=libclang_rt.asan.so)"

if [ ${1: -2} == ".c" ]; then

clang $CFLAGS $PASS_FLAGS -fpass-plugin=$SANITIZER_PASS -lprecompute $1 $LIBS
clang $CFLAGS -o a.out_original $1 $LIBS

elif [ ${1: -4} == ".cpp" ]; then
clang++ $CXXFLAGS $PASS_FLAGS -fpass-plugin=$SANITIZER_PASS -lprecompute $1 $LIBS
clang++ $CXXFLAGS -o a.out_original $1 $LIBS
else
echo "Unknown file suffix, use this script with .c or .cpp files"
fi
