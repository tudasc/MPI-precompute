#!/bin/bash

# location of this script
# this is the location where tha path file to introduce a datarace is
LULESH_PATCH_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# parameters that can be used for a sample invocation of the mini app
# used to test if the injected datarace is still found
TEST_INVOCATION_PARAMETER="-s 10 -i 3"
APP_NAME="LULESH"

# $1 : directory to download into
download(){
  echo "download"
  git clone https://github.com/LLNL/LULESH.git $1
  # set the specific commit we used
  # probably not necessary
  ( cd $1 && git checkout 3e01c40b3281aadb7f996525cdd4a3354f6d3801 )
}

# patches in a datarace
# $1 : directory with src (same argument as given to download dir)
patch_datarace(){
  echo "patch to inject datarace"
  patch $1/lulesh.cc $LULESH_PATCH_DIR/introduce_datarace.patch
}

# reverse the patch
# $1 : directory with src (same argument as given to download dir)
unpatch_datarace(){
  echo "reverse data race injection"
  patch -R $1/lulesh.cc $LULESH_PATCH_DIR/introduce_datarace.patch
}

# build without tsan
# $1 : directory with src (same argument as given to download dir)
# $2 : output file
build_vanilla(){
  echo "build without tsan"
  (cd $1 &&\
  mkdir -p build_vanilla &&\
  cd build_vanilla &&\
  # clean up any previous build
  rm -rf * &&\
  export USE_COMPILER_PASS=false &&\
  cmake -DCMAKE_CXX_COMPILER=$CLANG_WRAP_CXX -DWITH_MPI=Off -DCMAKE_CXX_FLAGS="-O2 -flto -fwhole-program-vtables -fuse-ld=lld" .. && \
  make &&\
  cp lulesh2.0 $2)
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
    cmake -DCMAKE_CXX_COMPILER=$CLANG_WRAP_CXX -DWITH_MPI=Off -DCMAKE_CXX_FLAGS="-O2 -fsanitize=thread -flto -fwhole-program-vtables -fuse-ld=lld" .. && \
    make &&\
    cp lulesh2.0 $2)
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
    cmake -DCMAKE_CXX_COMPILER=$CLANG_WRAP_CXX -DWITH_MPI=Off -DCMAKE_CXX_FLAGS="-O2 -fsanitize=thread -flto -fwhole-program-vtables -fuse-ld=lld" .. && \
    #endable our pass
    export USE_COMPILER_PASS=true &&\
    make &&\
    cp lulesh2.0 $2)
}