#!/bin/bash

# this script builds openmpi with the patch
MPI_SRC_PATH=/home/tj75qeje/openmpi_test_install/openmpi-4.1.1

configure_prefix="/home/tj75qeje/openmpi_test_install/install"

# the script is suppored to be execuited in the dir with the patch
OMPI_PATCH_DIR=$(pwd)

#TODO we need a switch if the original repo is in a clean state?

# check if a compatible version is used
# we tested 4.1.1


grep "major=4" $MPI_SRC_PATH/VERSION
if [ $? -ne 0 ]; then
echo "MPI Version is not matching"
exit 1
fi

grep "minor=1" $MPI_SRC_PATH/VERSION
if [ $? -ne 0 ]; then
echo "MPI Version is not matching"
exit 1
fi

grep "release=1" $MPI_SRC_PATH/VERSION
if [ $? -ne 0 ]; then
echo "MPI Version is not matching"
exit 1
fi

# patch makefile
patch $MPI_SRC_PATH/ompi/mca/osc/ucx/Makefile.in patches/ompi_makefile.patch

# patch ucx component initialization to activate more features of ucx
patch $MPI_SRC_PATH/ompi/mca/osc/ucx/osc_ucx_component.c patches/ompi_osc_ucx_component.patch


# patch the header

start_marker="// START INTERFACE MPIOPT"
end_marker="// END INTERFACE MPIOPT"

input_file=interface.h
output_file=$MPI_SRC_PATH/ompi/include/mpi.h.in

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

# May need to change with different openmpi version:
# the insertion line is an empty line and will be duplicated (before and after the inserted content)
insertion_line=2858

temp_file=$(mktemp)
head -n $insertion_line "$output_file" > "$temp_file"
echo "$extracted_content" >> "$temp_file"
tail -n +$insertion_line "$output_file" >> "$temp_file"
mv "$temp_file" "$output_file"

# end patch header

cd $MPI_SRC_PATH
# configuration of debug build:
./configure CC=clang CXX=clang++ CFLAGS="-g -Og -fno-eliminate-unused-debug-symbols" LDFLAGS="-g" CXX_FLAGS="-g -Og -fno-eliminate-unused-debug-symbols" --prefix=$configure_prefix --enable-debug --enable-mpi-cxx --enable-cxx-exceptions --enable-heterogeneous --enable-mpi1-compatibility --enable-static --with-hwloc=${HWLOC_ROOT} --with-slurm=/opt/slurm/current --with-pmi=/opt/slurm/current --with-ucx --enable-mca-no-build=btl-uct  --without-verbs --enable-mpi-fortran=no


cd $OMPI_PATCH_DIR
# build the patch
export MPI_SRC_PATH=$MPI_SRC_PATH
make install

cd $MPI_SRC_PATH

make -j && make install

# back to the current dir
cd $OMPI_PATCH_DIR







