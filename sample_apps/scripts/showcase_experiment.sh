#!/bin/bash

#prerequisites: llvm/16
# the configuration I used:
#git clone https://github.com/llvm/llvm-project.git
#git checkout llvmorg-16.0.1
#mkdir llvm_build && cd llvm_build
#cmake -DCMAKE_INSTALL_PREFIX=INSTALL_PREFIX -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=On -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;comiler-rt;lld;lldb;openmp;polly;pstl;mlir;flang" ../llvm-project/llvm/

# open openmpi-patch/Cmakelist.txt and adjust openmpi build settings if necessary
# execution in the Repository main dir
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j

source setup_env.sh
# run tests
ctest || exit

mkdir murb
cd ../sample_apps/MUrB

# if not done so already:
#git submodule update --init --recursive

# build example application for measurement
mkdir build
cd build

cmake .. -DCMAKE_CXX_COMPILER=$MPICXX -DCMAKE_CXX_FLAGS="-fopenmp -O3 -fuse-ld=lld -flto -fwhole-program-vtables" -DENABLE_MURB_MPI=ON -DENABLE_VISU=OFF -DENABLE_MURB_READER=OFF
make -j
# test if it runs
mpirun -n 2 ./bin/murb -v --im 100 -i 10 -n 100 || exit
cp ./bin/murb ../../../build/murb/murb_original

# clean up
rm -rf *
# activate pass
export USE_MPI_COMPILER_ASSISTANCE_PASS=true
cmake .. -DCMAKE_CXX_COMPILER=$MPICXX -DCMAKE_CXX_FLAGS="-fopenmp -O3 -fno-inline -fuse-ld=lld -flto -fwhole-program-vtables" -DENABLE_MURB_MPI=ON -DENABLE_VISU=OFF -DENABLE_MURB_READER=OFF
make -j
# test if it runs
mpirun -n 2 ./bin/murb -v --im 100 -i 10 -n 100 || exit
cp ./bin/murb ../../../build/murb/murb_altered

exit
# next steps are manual set up of job-scripts
# go to sample_apps/scripts/MUrB/MUrB_config.sh
# and amend the paths:

# where tor store experiments:
export EXPERIMENT_DIR=/work/scratch/tj75qeje/mpi-comp-match/MUrB

# paths based on repo base
export APPLICATION_ORIGINAL=$REPO_BASE/build/murb/murb_original
export APPLICATION_ALTERED=$REPO_BASE/build/murb/murb_altered
export VARIABLE_PARAM_FILE=$REPO_BASE/scripts/MUrB/parameters_MUrB.txt

# inspect sample_apps/scripts/SLURM_Header.sh and amend to fit your system if necessary

# generate job scripts in EXPERIMENT DIR
sample_apps/scripts/submit_jobs.sh MUrB/MUrB_config.sh

# submit each script one time with sbatch
sample_apps/scripts/submit_jobs.sh MUrB/MUrB_config.sh 1