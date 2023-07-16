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

objdump -T ./a.out | grep -q MPIOPT
NO_MPI_OPT= $?
# 1 if grep was empty, 0 if functions starting with MPIOPT are used

if [[ "EXPECT_MPIOPT" == 1 && "$NO_MPI_OPT" == 1 ]]; then
  echo "Expected MPIOPT usage but none found"
  exit 1
fi
if [[ "EXPECT_MPIOPT" == 0 && "$NO_MPI_OPT" == 0 ]]; then
  echo "Expected NOP MPIOPT usage but found usage"
  exit 1
fi

mpirun -n 2 ./a.out_original
mpirun -n 2 ./a.out

rm a.out a.out_original




