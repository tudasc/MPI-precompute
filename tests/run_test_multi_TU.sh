#!/bin/bash

echo "$(pwd)"

TEST_DIR=$1

source ./setup_env.sh

if [ ! -d "$TEST_DIR" ]; then
  echo "$TEST_DIR is not a directory"
  exit 1
fi

export MPI_COMPILER_ASSISTANCE_FRONTEND_PLUGIN_FILE="$TEST_DIR/plugin_data.json"

cd $TEST_DIR

make

mpirun -n 2 ./a.out | grep "Usage of MPIOPT optimized communication sceme"
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

# rebuild with our compiler pass enabled
make clean
export LFLAGS="-Og -g -fpass-plugin=$MPI_COMPILER_ASSISTANCE_PASS"
make
#check if compiler analysis was successfull
EXPECT_MPIOPT=1


mpirun -n 2 ./a.out | grep "Usage of MPIOPT optimized communication sceme"
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

make clean




