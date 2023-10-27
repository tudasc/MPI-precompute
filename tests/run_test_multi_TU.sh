#!/bin/bash


echo $(pwd)
# where the wrappers are found
BINARY_DIR=$1

# the testcase to use
TEST_DIR=$2

source ${BINARY_DIR}/setup_env.sh

if [ ! -d "$TEST_DIR" ]; then
  echo "$TEST_DIR is not a directory"
  exit 1
fi

cd $TEST_DIR
mkdir build
cd build

# without pass enabled to get the original application
export USE_MPI_COMPILER_ASSISTANCE_PASS=false
export OMPI_CC=clang
export OMPI_CXX=clang++

cmake -DCMAKE_C_COMPILER=clang -DCMAKE_C_FLAGS="-flto -fuse-ld=lld -fwhole-program-vtables" -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS="-flto -fuse-ld=lld -fwhole-program-vtables" $TEST_DIR
make

mpirun -n 2 ./test | grep "Usage of MPIOPT optimized communication sceme"
SAVE_STATUS=( "${PIPESTATUS[@]}" )
MPIRUN_STATUS=${SAVE_STATUS[0]}
NO_MPI_OPT=${SAVE_STATUS[1]}

if [[ "$MPIRUN_STATUS" != 0 ]]; then
  echo "Crash of ORIGINAL result application - testcase probably broken"
  exit 1
fi

# 1 if grep was empty, 0 otherwise
if [[ "$NO_MPI_OPT" != 1 ]]; then
  echo "Expected NO MPIOPT in unaltered application usage but found usage"
  exit 1
fi

# rebuild with pass enabled
# also remove all cmake file to start build process from scratch
rm -rf *

export USE_MPI_COMPILER_ASSISTANCE_PASS=true
export OMPI_CC="${BINARY_DIR}/clang_wrap_cc"
export OMPI_CXX="${BINARY_DIR}/clang_wrap_cxx"

cmake -DCMAKE_C_COMPILER="${BINARY_DIR}/clang_wrap_cc" -DCMAKE_C_FLAGS="-flto -fuse-ld=lld -fwhole-program-vtables" -DCMAKE_CXX_COMPILER="${BINARY_DIR}/clang_wrap_cxx" -DCMAKE_CXX_FLAGS="-flto -fuse-ld=lld -fwhole-program-vtables" $TEST_DIR

make

EXPECT_MPIOPT=1

mpirun -n 2 ./test | grep "Usage of MPIOPT optimized communication sceme"
SAVE_STATUS=( "${PIPESTATUS[@]}" )
MPIRUN_STATUS=${SAVE_STATUS[0]}
NO_MPI_OPT=${SAVE_STATUS[1]}

if [[ "$MPIRUN_STATUS" != 0 ]]; then
  echo "Crash of result application"
  exit 1
fi

if [[ "$EXPECT_MPIOPT" == 1 && "$NO_MPI_OPT" != 0 ]]; then
  echo "Expected MPIOPT usage but none found"
  exit 1
fi
if [[ "$EXPECT_MPIOPT" == 0 && "$NO_MPI_OPT" != 1 ]]; then
  echo "Expected NO MPIOPT usage but found usage"
  exit 1
fi


