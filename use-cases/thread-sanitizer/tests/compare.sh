#!/bin/bash

echo $(pwd)
# where the wrappers are found
BINARY_DIR=$1

# the testcase to use
TEST_CASE=$2
DRB_DIR=$(dirname $TEST_CASE)


source ${BINARY_DIR}/setup_env.sh

#compares standard thread sanitizer with the precomputed one

GREP_STRING="WARNING: ThreadSanitizer: data race"

rm ./a.out ./a.out_original

CFLAGS="-O2 -g -fopenmp -fsanitize=thread"
PASS_FLAGS="-fuse-ld=lld -flto -fwhole-program-vtables -fno-inline"


# compile
if grep -q 'PolyBench' "$TEST_CASE"; then
    # needs additional compiler flags
    POLYFLAG="-I$DRB_DIR -I$DRB_DIR/utilities -DPOLYBENCH_NO_FLUSH_CACHE -DPOLYBENCH_TIME -D_POSIX_C_SOURCE=200112L"
    CFLAGS="$CFLAGS $POLYFLAG";
    # normal compilation
    export USE_COMPILER_PASS=false
    $CLANG_WRAP_CC $CFLAGS -c -o polybench.o $DRB_DIR/utilities/polybench.c
    $CLANG_WRAP_CC $CFLAGS -c -o main.o $2
    $CLANG_WRAP_CC $CFLAGS -o ./a.out_original main.o polybench.o
    # with pass
    export USE_COMPILER_PASS=true
    $CLANG_WRAP_CC $CFLAGS $PASS_FLAGS -c -o polybench.o $DRB_DIR/utilities/polybench.c
    $CLANG_WRAP_CC $CFLAGS $PASS_FLAGS -c -o main.o $2
    $CLANG_WRAP_CC $CFLAGS $PASS_FLAGS -o ./a.out main.o polybench.o

else
    # normal compilation
    export USE_COMPILER_PASS=false
    $CLANG_WRAP_CC $CFLAGS -o ./a.out_original $2
    # with pass
    export USE_COMPILER_PASS=true
    $CLANG_WRAP_CC $CFLAGS $PASS_FLAGS -o ./a.out $2
fi


# execution
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
    exit -2
fi

# should never reach this
exit -1



