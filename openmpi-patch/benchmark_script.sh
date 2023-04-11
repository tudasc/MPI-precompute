#!/bin/bash

# same as -n
#SBATCH --ntasks 2

#SBATCH --mem-per-cpu=10
#same as -t
#SBATCH --time 00:10:00



#same as -c
#SBATCH --cpus-per-task 96
# one node per process

#same as -j
#SBATCH --job-name ucx_Testing
#same as -o
#SBATCH --output job.out
#same as -e
#SBATCH --error job.err

#here the jobscript starts


srun hostname

export OMPI_MCA_opal_warn_on_missing_libcuda=0
export OMPI_MCA_opal_common_ucx_opal_mem_hooks=1
export OMPI_MCA_osc=ucx
export OMPI_MCA_pml=ucx
export UCX_WARN_UNUSED_ENV_VARS=n
export UCX_UNIFIED_MODE=y

ml gcc/8.3.1 hwloc/2.5.0 openucx/1.12.0 openmpi/test


for strat in {0..3}
do
    for data in {0..2}
    do
        for iters in {10..100..10}
        do
            for size in {10..100..10}
            do
                for count in {10..100..10}
                do
                    for threshold in {0..$size..5}
                    do
                        srun ./benchmark --iters $iters --size $size --count $count --threshold $threshold --strategy $strat --data $data
                    done
                done
            done
        done
    done
done