#!/bin/bash

echo $(pwd)
# where the wrappers are found
BINARY_DIR=$1

# the directory with the drb testcases
TEST_CASE_DIR=$2


TEST_CASES=$(ls $TEST_CASE_DIR)


source ${BINARY_DIR}/setup_env.sh
RUN_SCRIPT=$BINARY_DIR/run.sh

#compares standard thread sanitizer with the precomputed one

GREP_STRING="WARNING: ThreadSanitizer: data race"

# the result file
echo "time_original,time_precompute" > timing.csv

for TEST_CASE in $TEST_CASES
do
  if [[ $TEST_CASE == *.c ]]; then


rm ./a.out ./a.out_original

if grep -q 'PolyBench' "$TEST_CASE_DIR/$TEST_CASE"; then
  echo "Skip testcase for now, it needs different compile flags"
#  additional_compile_flags+=" $POLYFLAG";
continue
fi



POLYFLAG="micro-benchmarks/utilities/polybench.c -I micro-benchmarks -I micro-benchmarks/utilities -DPOLYBENCH_NO_FLUSH_CACHE -DPOLYBENCH_TIME -D_POSIX_C_SOURCE=200112L"


echo "case $TEST_CASE"

# compile
$RUN_SCRIPT $TEST_CASE_DIR/$TEST_CASE &>/dev/null

if [[ -x "./a.out" ]]; then
    # a.out exists
/usr/bin/time -f "%e" -o time_orig.log --quiet ./a.out_original &> orig.log
/usr/bin/time -f "%e" -o time_precompute.log --quiet ./a.out &> precompute.log
if grep -qF "$GREP_STRING" orig.log; then

        if grep -qF "$GREP_STRING" precompute.log; then
          # success
          time_original=$(cat time_orig.log)
          time_precompute=$(cat time_precompute.log)
          echo "both versions found the datarace"
          echo "$time_original,$time_precompute" >> timing.csv

        else
            echo "Original sanitizer found the data race but precomputed not"

        fi
    else
        #echo "Original sanitizer found no race"
        if grep -qF "$GREP_STRING" precompute.log; then
                  echo "Original sanitizer found no race but precomputed did"
                else
                  #success
                    time_original=$(cat time_orig.log)
                    time_precompute=$(cat time_precompute.log)
                    echo "both versions found no race"
                    echo "$time_original,$time_precompute" >> timing.csv
        fi
    fi
else
    echo "Compilation fail"
fi

fi

done



