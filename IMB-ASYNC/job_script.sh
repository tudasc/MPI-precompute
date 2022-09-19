#!/bin/bash

# same as -n
#SBATCH --ntasks 2

#SBATCH --mem-per-cpu=3800
#same as -t
###SBATCH --time 00:30:00
# approx half a minute per benchmark, 4 benchmarks * 28 parameters
# approx 2h
#SBATCH --time 2:00:00

#SBATCH --tasks-per-node 1
# one node per process


###SBATCH --array 0-1
#SBATCH --array 1-100
###SBATCH --array 0-10

#same as -j
#SBATCH --job-name MPI-ASYNC-BENCHMARK
#same as -o
#SBATCH --output output/job_%a.out
## The real output will be saved into yml files
#SBATCH --output /dev/null



MODE=${MODE-normal}
# 0,1,2,3 for normal,rendevouz1,rendevouz2,eager
MOD=${MOD-0}


# config
OUTPATH=/work/scratch/tj75qeje/mpi-comp-match/output/$SLURM_NPROCS
mkdir -p $OUTPATH


TIMEOUT_CMD="/usr/bin/timeout -k 60 60"
#here the jobscript starts
#srun hostname

ml purge
ml gcc/8.3.1
ml hwloc/2.5.0 clang/11.1.0
ml openucx/1.12.0

if [[ "$MO" -eq 0 ]]; then
ml openmpi/normal
elif [[ "$MOD" -eq 1 ]]; then
ml openmpi/rendevouz1
elif [[ "$MOD" -eq 2 ]]; then
ml openmpi/rendevouz2
else
ml openmpi/eager
fi

#export OMPI_MCA_opal_warn_on_missing_libcuda=0
#export OMPI_MCA_opal_common_ucx_opal_mem_hooks=1
export OMPI_MCA_osc=ucx
export OMPI_MCA_pml=ucx
export UCX_WARN_UNUSED_ENV_VARS=n
export UCX_UNIFIED_MODE=y

I=0
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/tj75qeje/mpi-comp-match/IMB-ASYNC/src_cpp/ASYNC/thirdparty/lib/

mkdir -p $OUTPATH

# random parameterorder, to add random disturbance (e.g. the slurm controler will interrupt quite regularly)
RAND_PARAMS=$(shuf /home/tj75qeje/mpi-comp-match/IMB-ASYNC/parameters.txt)
# change delimiter (IFS) to new line.
IFS_BAK=$IFS
IFS=$'\n'
for PARAM in $RAND_PARAMS; do

# return delimiter to previous value
IFS=$IFS_BAK
IFS_BAK=

((I=I+1))

# get calctime out of param, as calctime will not be included inside the yml, we need to include it in the filename
CALCTIME=$(echo $PARAM | cut -d' ' -f2)

for CYCLES in $(seq 2 2 64); do

$TIMEOUT_CMD srun --cpu-bind=cores ./IMB-ASYNC_orig async_persistentpt2pt -cper10usec 64 -workload calc -thread_level single -datatype char -ncycles $CYCLES -nwarmup 0 $PARAM -output $OUTPATH/${MODE}_calctime_${CALCTIME}_cycles_${CYCLES}.$SLURM_JOB_ID.$SLURM_ARRAY_TASK_ID.$I.yaml >& /dev/null

done # done for CYCLEs

# change delimiter (IFS) to new line.
IFS_BAK=$IFS
IFS=$'\n'
done 
# return delimiter to previous value
IFS=$IFS_BAK
IFS_BAK=


echo "done"

