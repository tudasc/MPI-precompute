#!/bin/bash

echo $(pwd)
# where the wrappers are found
BINARY_DIR=$1

# the testcase to use
TEST_CASE=$2
source ${BINARY_DIR}/setup_env.sh

#compares standard thread sanitizer with the precomputed one

GREP_STRING="WARNING: ThreadSanitizer: data race"

rm ./a.out

COMPILER_FLAGS="-O2 -g -fopenmp -flto -fuse-ld=lld -ltsan"

# compile
# with pass
export USE_COMPILER_PASS=true
$BINARY_DIR/flang_wrap $COMPILER_FLAGS -o ./a.out $2

# execution
if [[ -x "./a.out" ]]; then
    # a.out exists
    if ./a.out 2>&1 | grep -qF "$GREP_STRING"; then
        # found data race
        echo "found the datarace"
        exit 0
    else
        echo "did not find the race"
        exit -1
    fi
else
    echo "Compilation fail"
    exit -2
fi

# should never reach this
exit -1



