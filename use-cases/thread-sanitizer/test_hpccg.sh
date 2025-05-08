#!/bin/bash

# call with llvm-reduce -j 6 --test test_hpccg.sh full_ir.bc

INPUT_IR=$1

#temporary file
EXECUTABLE_FILE=$(mktemp)


echo "compile $INPUT_IR"
LD_PRELOAD="$(clang -print-file-name=libclang_rt.asan.so)" clang++ -x ir $INPUT_IR -fpass-plugin=/home/tim/precompute/cmake-build-debug/use-cases/thread-sanitizer/sanitizer_precompute_compiler_pass/libsanitizer_precompute_pass.so -lprecompute -fsanitize=thread -flto -fwhole-program-vtables -fuse-ld=lld -fopenmp=libomp -rdynamic -o $EXECUTABLE_FILE 2> /dev/null

#note that we grep in stderr
$EXECUTABLE_FILE 3 3 3 |& grep "ERROR: ThreadSanitizer:"
EXIT_CODE=$?

#cleanup
rm $EXECUTABLE_FILE

exit $EXIT_CODE
