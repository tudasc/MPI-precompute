#!/bin/bash

echo "$(pwd)"
RUN_SCRIPT=$(pwd)/run.sh

TEST_DIR=$1

source ./setup_env.sh

if [ ! -d "$TEST_DIR" ]; then
  echo "$TEST_DIR is not a directory"
  exit 1
fi

export MPI_COMPILER_ASSISTANCE_FRONTEND_PLUGIN_FILE="$TEST_DIR/plugin_data.json"

cd $TEST_DIR

$RUN_SCRIPT *.c

#check if compiler analysis cas successfull

EXPECT_MPIOPT=1

mpirun -n 2 ./a.out_original | grep "Usage of MPIOPT optimized communication sceme"
NO_MPI_OPT=$?
# 1 if grep was empty, 0 otherwise
if [[ "$NO_MPI_OPT" == 0 ]]; then
  echo "Expected NO MPIOPT in unaltered application usage but found usage"
  exit 1
fi

mpirun -n 2 ./a.out | grep "Usage of MPIOPT optimized communication sceme"
NO_MPI_OPT=$?
echo " grep return code: ${NO_MPI_OPT}"
if [[ "$EXPECT_MPIOPT" == 1 && "$NO_MPI_OPT" == 1 ]]; then
  echo "Expected MPIOPT usage but none found"
  exit 1
fi
if [[ "$EXPECT_MPIOPT" == 0 && "$NO_MPI_OPT" == 0 ]]; then
  echo "Expected NO MPIOPT usage but found usage"
  exit 1
fi

rm a.out a.out_original




