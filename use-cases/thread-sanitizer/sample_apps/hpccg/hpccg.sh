#!/bin/bash

# location of this script
# this is the location where tha path file to introduce a datarace is
HPCCG_PATCH_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# parameters that can be used for a sample invocation of the mini app
# used to test if the injected datarace is still found
TEST_INVOCATION_PARAMETER="3 3 3"
APP_NAME="HPCCG"

# $1 : directory to download into
download(){
  echo "download"
  git clone https://github.com/Mantevo/HPCCG.git $1
  # set the specific commit we used
  # probably not necessary
  ( cd $1 && git checkout 80dd2f12a4e8aa70c330a5686cdda3fd187c2545 )
  # patch makefile
  patch $1/Makefile $HPCCG_PATCH_DIR/Makefile.patch
    # patch application to remove datarace
  patch $1/main.cpp $HPCCG_PATCH_DIR/remove_datarace.patch
}

# patches in a datarace
# $1 : directory with src (same argument as given to download dir)
patch_datarace(){
  echo "patch to inject datarace"
  # re-introduce the datarace present in original code
  patch -R $1/main.cpp $HPCCG_PATCH_DIR/remove_datarace.patch
}

# reverse the patch
# $1 : directory with src (same argument as given to download dir)
unpatch_datarace(){
  echo "reverse data race injection"
  patch $1/main.cpp $HPCCG_PATCH_DIR/remove_datarace.patch
}

# build without tsan
# $1 : directory with src (same argument as given to download dir)
# $2 : output file
build_vanilla(){
  echo "build without tsan"
  (cd $1 &&\
    # clean up any previous build
    make clean &&\
    export USE_COMPILER_PASS=false &&\
    make &&\
    cp test_HPCCG $2)
}

# build with normal tsan
# $1 : directory with src (same argument as given to download dir)
# $2 : output file
build_tsan_normal(){
  echo "build normal"
  (cd $1 &&\
    # clean up any previous build
    make clean &&\
    export USE_COMPILER_PASS=false &&\
    export SANITIZE_FLAG="-fsanitize=thread" &&\
    make &&\
    cp test_HPCCG $2)
}

# build with modified tsan
# $1 : directory with src (same argument as given to download dir)
# $2 : output file
build_tsan_modified(){
  echo "build modified"
  (cd $1 &&\
    # clean up any previous build
    make clean &&\
    export USE_COMPILER_PASS=true &&\
    export SANITIZE_FLAG="-fsanitize=thread" &&\
    make &&\
    cp test_HPCCG $2)
}