#!/bin/bash

#CFLAGS="-std=c11 -O3 ${INCLUDE}"
CFLAGS="-std=c11 -O1 -g ${INCLUDE}"
LIBS="-lm"
CXXFLAGS="-std=c++17 -O1 -g ${INCLUDE}"
#

#PASS_FLAGS="-flto -fwhole-program-vtables"
PASS_FLAGS="-fuse-ld=lld -flto -fwhole-program-vtables"


export MPI_COMPILER_ASSISTANCE_FRONTEND_PLUGIN_FILE="plugin_data.json"
#
if [ ${1: -2} == ".c" ]; then

OMPI_CC=$CLANG_WRAP_CC $MPICC $CFLAGS $PASS_FLAGS $1 $LIBS
OMPI_CC=clang $MPICC $CFLAGS -o a.out_original $1 $LIBS

elif [ ${1: -4} == ".cpp" ]; then
OMPI_CXX=$CLANG_WRAP_CXX $MPICXX $CXXFLAGS $PASS_FLAGS $1 $LIBS
OMPI_CXX=clang++ $MPICXX $CXXFLAGS -o a.out_original $1 $LIBS
else
echo "Unknown file suffix, use this script with .c or .cpp files"
fi
