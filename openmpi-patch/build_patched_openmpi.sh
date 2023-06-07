#!/bin/bash

# this script builds openmpi with the patch
MPI_SRC_DIR=/home/tj75qeje/openmpi_test_install/openmpi-4.1.1

configure_prefix="/home/tj75qeje/openmpi_test_install/install"

configure_vars='CC=clang CXX=clang++ CFLAGS="-g -Og -fno-eliminate-unused-debug-symbols" LDFLAGS="-g" CXX_FLAGS="-g -Og -fno-eliminate-unused-debug-symbols"  '

configure_args='--enable-debug --enable-mpi-cxx --enable-cxx-exceptions --enable-heterogeneous --enable-mpi1-compatibility --enable-static --with-hwloc=${HWLOC_ROOT} --with-slurm=/opt/slurm/current --with-pmi=/opt/slurm/current --with-ucx --enable-mca-no-build=btl-uct  --without-verbs --enable-mpi-fortran=no'

# the script is suppored to be execuited in the dir with the patch
OMPI_PATCH_DIR=${pwd}

#TODO we need a switch if the original repo is in a clean state?

# check if a compatible version is used
# we tested 4.1.1

grep "major=4" $MPI_SRC_DIR/VERSION
if [ $? -ne 0 ]; then
echo "MPI Version is not matching"
exit 1
fi

grep "minor=1" $MPI_SRC_DIR/VERSION
if [ $? -ne 0 ]; then
echo "MPI Version is not matching"
exit 1
fi

grep "release=1" $MPI_SRC_DIR/VERSION
if [ $? -ne 0 ]; then
echo "MPI Version is not matching"
exit 1
fi

# patch makefile
patch [options] $MPI_SRC_DIR/ompi/mca/osc/ucx/Makefile.in patches/ompi_makefile.patch 

# patch ucx component initialization to activate more features of ucx
patch [options] $MPI_SRC_DIR/ompi/mca/osc/ucx/osc_ucx_component.c patches/ompi_osc_ucx_component.patch 


# patch the header

start_marker="// START INTERFACE MPIOPT"
end_marker="// END INTERFACE MPIOPT"

$input_file=interface.h

# Find the line numbers of the start and end markers
start_line=$(grep -n "$start_marker" "$input_file" | cut -d ":" -f 1)
end_line=$(grep -n "$end_marker" "$input_file" | cut -d ":" -f 1)

# Check if the markers are present in the input file
if [ -z "$start_line" ] || [ -z "$end_line" ]; then
    echo "Header Begin/end Markers not found"
    exit 1
fi

# Extract the content between the markers
extracted_content=$(sed -n "${start_line},${end_line}p" "$input_file")

# May need to change with different openmpi version
insertion_line=2859

sed -i "${insertion_line}i\\${extracted_content}" $MPI_SRC_DIR/ompi/include/mpi.h.in

cd $MPI_SRC_DIR

$configure_vars ./configure --prefix=$configure_prefix $configure_args

cd $OMPI_PATCH_DIR
MPI_SRC_PATH=$MPI_SRC_DIR make install

cd $MPI_SRC_DIR

make -j && make install


cd $OMPI_PATCH_DIR







