#!/bin/bash


OUTPUT_DIR="/work/scratch/tj75qeje/precompute/lulesh"
source ../lulesh/lulesh.sh

APP_NAME=LULESH




if [ ! -d "$APP_NAME" ]; then
  # download if necessary
  download $APP_NAME
fi

EXECUTABLE_DIR=$PWD/$APP_NAME

build_vanilla $APP_NAME ${EXECUTABLE_DIR}/without.exe
build_tsan_normal $APP_NAME ${EXECUTABLE_DIR}/normal.exe
build_tsan_modified $APP_NAME ${EXECUTABLE_DIR}/modified.exe



mkdir -p $OUTPUT_DIR
# setup the csv header for the result data
echo "config,num_threads,mode,time"> $OUTPUT_DIR/000_header