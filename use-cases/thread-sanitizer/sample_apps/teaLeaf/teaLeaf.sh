#!/bin/bash

# location of this script
# this is the location where tha path file to introduce a datarace is
TEALEAF_PATCH_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# parameters that can be used for a sample invocation of the mini app
# used to test if the injected datarace is still found
TEST_INVOCATION_PARAMETER=""
APP_NAME="TeaLeaf"

# $1 : directory to download into
download(){
  echo "download"
  git clone https://github.com/UoB-HPC/TeaLeaf.git $1
  # set the specific commit we used
  # probably not necessary
  ( cd $1 && git checkout e70261c0be40537da75b258108ed2898f84f3c58 )
  # patch input file to have smaller problem size for testing
  patch $1/tea.in $TEALEAF_PATCH_DIR/problem_size.patch
}

# patches in a datarace
# $1 : directory with src (same argument as given to download dir)
patch_datarace(){
  echo "patch to inject datarace"
  # re-introduce the datarace present in original code
  patch $1/src/omp/cg.cpp $TEALEAF_PATCH_DIR/introduce_datarace.patch
}

# reverse the patch
# $1 : directory with src (same argument as given to download dir)
unpatch_datarace(){
  echo "reverse data race injection"
  patch -R $1/src/omp/cg.cpp $TEALEAF_PATCH_DIR/introduce_datarace.patch
}

# build without tsan
# $1 : directory with src (same argument as given to download dir)
# $2 : output file
build_vanilla(){
  echo "build without tsan"
  (cd $1 &&\
    mkdir -p build_normal &&\
    cd build_normal &&\
    # clean up any previous build
    rm -rf * &&\
    export USE_COMPILER_PASS=false &&\
    cmake -DMODEL=omp -DCMAKE_CXX_COMPILER=$CLANG_WRAP_CXX -DCMAKE_CXX_FLAGS="-flto -fwhole-program-vtables -fuse-ld=lld -O3" .. && \
    make &&\
    cp omp-tealeaf $2)
    # copy input files
    cp $1/tea.in $1/tea.problems $(dirname $2)
}

# build with normal tsan
# $1 : directory with src (same argument as given to download dir)
# $2 : output file
build_tsan_normal(){
  echo "build normal"
  (cd $1 &&\
    mkdir -p build_normal &&\
    cd build_normal &&\
    # clean up any previous build
    rm -rf * &&\
    export USE_COMPILER_PASS=false &&\
    cmake -DMODEL=omp -DCMAKE_CXX_COMPILER=$CLANG_WRAP_CXX -DCMAKE_CXX_FLAGS="-fsanitize=thread -flto -fwhole-program-vtables -fuse-ld=lld -O3" .. && \
    make &&\
    cp omp-tealeaf $2)
    # copy input files
    cp $1/tea.in $1/tea.problems $(dirname $2)
}

# build with modified tsan
# $1 : directory with src (same argument as given to download dir)
# $2 : output file
build_tsan_modified(){
  echo "build modified"
  (cd $1 &&\
    mkdir -p build_modified &&\
    # clean up any previous build
    cd build_modified &&\
    rm -rf * &&\
    export USE_COMPILER_PASS=false &&\
    cmake -DMODEL=omp -DCMAKE_CXX_COMPILER=$CLANG_WRAP_CXX -DCMAKE_CXX_FLAGS="-fsanitize=thread -flto -fwhole-program-vtables -fuse-ld=lld -O3" .. && \
    #endable our pass
    export USE_COMPILER_PASS=true &&\
    make &&\
    cp omp-tealeaf $2)
    # copy input files
    cp $1/tea.in $1/tea.problems $(dirname $2)
}