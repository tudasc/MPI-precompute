#!/bin/bash

#CFLAGS="-std=c11 -O3 ${INCLUDE}"
CFLAGS="-std=c11 -O1 -g ${INCLUDE}"
LIBS="-lopen-pal -lucp -lm"
CXXFLAGS="-std=c++17 -O1 -g ${INCLUDE}"

#PASS_FLAGS="-fwhole-program-vtables -fpass-plugin=$MPI_COMPILER_ASSISTANCE_PASS"
PASS_FLAGS="-flto -fwhole-program-vtables -fpass-plugin=$WHOLE_PROGRAM_DEVIRT_ANALYSIS_PASS"


export MPI_COMPILER_ASSISTANCE_FRONTEND_PLUGIN_FILE="plugin_data.json"
#
if [ ${1: -2} == ".c" ]; then

OMPI_CC=clang $MPICC $CFLAGS $PASS_FLAGS $1 $LIBS
OMPI_CC=clang $MPICC $CFLAGS -o a.out_original $1 $LIBS

elif [ ${1: -4} == ".cpp" ]; then
OMPI_CXX=clang++ $MPICXX $CXXFLAGS $PASS_FLAGS $1 $LIBS
OMPI_CXX=clang++ $MPICXX $CXXFLAGS -o a.out_original $1 $LIBS
else
echo "Unknown file suffix, use this script with .c or .cpp files"
fi
