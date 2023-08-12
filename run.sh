#!/bin/bash

#CFLAGS="-std=c11 -O3 ${INCLUDE}"
CFLAGS="-std=c11 -O0 -g ${INCLUDE}"
LIBS="-lopen-pal -lucp -lm"
CXXFLAGS="-std=c++17 -O0 -g ${INCLUDE}"


export MPI_COMPILER_ASSISTANCE_FRONTEND_PLUGIN_FILE="plugin_data.json"
#
if [ ${1: -2} == ".c" ]; then

OMPI_CC=clang $MPICC $CFLAGS -fpass-plugin=$MPI_COMPILER_ASSISTANCE_PASS $1 $LIBS
OMPI_CC=clang $MPICC $CFLAGS -o a.out_original $1 $LIBS

elif [ ${1: -4} == ".cpp" ]; then
OMPI_CXX=clang++ $MPICXX $CXXFLAGS -fpass-plugin=$MPI_COMPILER_ASSISTANCE_PASS  $1 $LIBS
OMPI_CXX=clang++ $MPICXX $CXXFLAGS -o a.out_original $1 $LIBS
else
echo "Unknown file suffix, use this script with .c or .cpp files"
fi
