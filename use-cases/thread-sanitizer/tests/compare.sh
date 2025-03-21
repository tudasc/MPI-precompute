#!/bin/bash

echo $(pwd)
# where the wrappers are found
BINARY_DIR=$1

# the testcase to use
TEST_CASE=$2

RUN_SCRIPT=$BINARY_DIR/run.sh

source ${BINARY_DIR}/setup_env.sh

#compares standard thread sanitizer with the precomputed one

GREP_STRING="WARNING: ThreadSanitizer: data race"

rm ./a.out ./a.out_original

# compile
$RUN_SCRIPT $TEST_CASE

if [[ -x "./a.out" ]]; then
    # a.out exists
    if ./a.out_original 2>&1 | grep -qF "$GREP_STRING"; then
        # original sanitizer found data race

        if ./a.out 2>&1 | grep -qF "$GREP_STRING"; then
          # success
          echo "both versions found the datarace"
            exit 0
        else
            echo "Original sanitizer found the data race but precomputed not"
            exit -1
        fi
    else
        #echo "Original sanitizer found no race"
        if ./a.out 2>&1 | grep -qF $GREP_STRING; then
                  echo "Original sanitizer found no race but precomputed did"
                    exit -1
                else
                  #success
                    echo "both versions found no race"
                    exit 0
                fi
    fi
else
    echo "Compilation fail"
    exit -1
fi

# should never reach this
exit -1



