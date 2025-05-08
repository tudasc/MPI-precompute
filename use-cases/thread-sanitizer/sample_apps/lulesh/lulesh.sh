#!/bin/bash

# location of this script
# this is the location where tha path file to introduce a datarace is
LULESH_PATCH_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# $1 : directory to download into
download(){
git clone https://github.com/LLNL/LULESH.git $1
# set the specific commit we used
# probably not necessary
( cd $1 && git checkout 3e01c40b3281aadb7f996525cdd4a3354f6d3801 )
}

# patches in a datarace
# $1 : directory with src (same argument as given to download dir)
patch_datarace(){
  patch $1/lulesh.cc $LULESH_PATCH_DIR/introduce_datarace.patch
}

# reverse the patch
# $1 : directory with src (same argument as given to download dir)
unpatch_datarace(){
  patch -R $1/lulesh.cc $LULESH_PATCH_DIR/introduce_datarace.patch
}

# build without tsan
# $1 : directory with src (same argument as given to download dir)
# $2 : output file
build_vanilla(){
  (cd $1 &&\
  mkdir -p build_vanilla &&\
  # clean up any previous build
  rm -rf* &&\
  export USE_COMPILER_PASS=false &&\
  cmake -DCMAKE_CXX_COMPILER=$CLANG_WRAP_CXX -DWITH_MPI=Off -DCMAKE_CXX_FLAGS="-O2 -flto -fwhole-program-vtables -fuse-ld=lld" .. && \
  make &&\
  cp lulesh2.0 $2)
}

# build with normal tsan
# $1 : directory with src (same argument as given to download dir)
# $2 : output file
build_tsan_normal(){
  (cd $1 &&\
    mkdir -p build_vanilla &&\
    # clean up any previous build
    rm -rf* &&\
    export USE_COMPILER_PASS=false &&\
    cmake -DCMAKE_CXX_COMPILER=$CLANG_WRAP_CXX -DWITH_MPI=Off -DCMAKE_CXX_FLAGS="-O2 -fsanitize=thread -flto -fwhole-program-vtables -fuse-ld=lld" .. && \
    make &&\
    cp lulesh2.0 $2)
}

# build with modified tsan
# $1 : directory with src (same argument as given to download dir)
# $2 : output file
build_tsan_modified(){
  (cd $1 &&\
    mkdir -p build_vanilla &&\
    # clean up any previous build
    rm -rf* &&\
    export USE_COMPILER_PASS=false &&\
    cmake -DCMAKE_CXX_COMPILER=$CLANG_WRAP_CXX -DWITH_MPI=Off -DCMAKE_CXX_FLAGS="-O2 -fsanitize=thread -flto -fwhole-program-vtables -fuse-ld=lld" .. && \
    #endable our pass
    export USE_COMPILER_PASS=true &&\
    make &&\
    cp lulesh2.0 $2)
}