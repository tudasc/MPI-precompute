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

mpirun -n 2 ./a.out_original
mpirun -n 2 ./a.out

rm a.out a.out_original




