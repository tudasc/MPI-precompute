#!/bin/bash


INSTALL_PATH=/home/tj75qeje/mpi-comp-match_sample_codes

WD=$(pwd)

PATCH_FILE=$WD/MUrB.patch

if ! [ -f "$PATCH_FILE" ]; then
echo "Error: could not find patch file"
exit -1
fi

#original
git clone https://github.com/kouchy/MUrB.git $INSTALL_PATH/MUrB_original
mkdir $INSTALL_PATH/MUrB_original/build
cd $INSTALL_PATH/MUrB_original

git submodule update --init --recursive
cd build
cmake .. -DCMAKE_CXX_FLAGS="-O3 -funroll-loops -mtune=native -march=native -fopenmp" -DENABLE_MURB_MPI=ON -DENABLE_VISU=OFF -DENABLE_MURB_READER=OFF
make -j

# altered
git clone https://github.com/kouchy/MUrB.git $INSTALL_PATH/MUrB
mkdir $INSTALL_PATH/MUrB/build
cd $INSTALL_PATH/MUrB

git submodule update --init --recursive
git apply $PATCH_FILE

cd build
cmake .. -DCMAKE_CXX_FLAGS="-O3 -funroll-loops -mtune=native -march=native -fopenmp" -DENABLE_MURB_MPI=ON -DENABLE_VISU=OFF -DENABLE_MURB_READER=OFF
make -j


cd $WD
