#!/bin/bash
echo $(pwd)
# where the wrappers are found
BINARY_DIR=$1

# the testcase to use
TEST_DIR=$2

RUN_SCRIPT=$BINARY_DIR/run.sh


source $BINARY_DIR/setup_env.sh

if [ ! -d "$TEST_DIR" ]; then
  echo "$TEST_DIR is not a directory"
  exit 1
fi

cd $TEST_DIR

export USE_COMPILER_PASS=true

if ls *.cpp 1> /dev/null 2>&1; then
  $RUN_SCRIPT *.cpp
else
  $RUN_SCRIPT *.c
fi

#check if compiler analysis was successfull
EXPECT_NO_MPIOPT=${EXPECT_NO_MPIOPT:=0}

mpirun -n 2 ./a.out_original $ARGS_TO_TEST_EXEC | grep "Usage of MPIOPT optimized communication sceme"
SAVE_STATUS=( "${PIPESTATUS[@]}" )
MPIRUN_STATUS=${SAVE_STATUS[0]}
NO_MPI_OPT=${SAVE_STATUS[1]}

rm a.out_original

if [[ "$MPIRUN_STATUS" != 0 ]]; then
  echo "Crash of ORIGINAL result application - testcase probably broken"
  exit 1
fi


# 1 if grep was empty, 0 otherwise
if [[ "$NO_MPI_OPT" != 1 ]]; then
  echo "Expected NO MPIOPT in unaltered application usage but found usage"
  exit 1
fi

echo "mpirun -n 2 ./a.out $ARGS_TO_TEST_EXEC"
mpirun -n 2 ./a.out $ARGS_TO_TEST_EXEC | grep "Usage of MPIOPT optimized communication sceme"
SAVE_STATUS=( "${PIPESTATUS[@]}" )
MPIRUN_STATUS=${SAVE_STATUS[0]}
NO_MPI_OPT=${SAVE_STATUS[1]}

rm *.out

if [[ "$MPIRUN_STATUS" != 0 ]]; then
  echo "Crash of result application"
  exit 1
fi

if [[ "$EXPECT_NO_MPIOPT" == 0 && "$NO_MPI_OPT" != 0 ]]; then
  echo "Expected MPIOPT usage but none found"
  exit 1
fi
if [[ "$EXPECT_NO_MPIOPT" == 1 && "$NO_MPI_OPT" != 1 ]]; then
  echo "Expected NO MPIOPT usage but found usage"
  exit 1
fi
