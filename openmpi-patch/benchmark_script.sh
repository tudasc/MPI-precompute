#!/bin/bash

# same as -n
#SBATCH --ntasks 2

#SBATCH --mem-per-cpu=10
#same as -t
#SBATCH --time 00:01:00



#same as -c
#SBATCH --cpus-per-task 96
# one node per process

#SBATCH --array=1000,2000,3000,4000,5000,6000,7000,8000,9000,10000

#same as -j
#SBATCH --job-name ucx_Testing
#same as -o
#SBATCH --output /home/td37cari/job.out
#same as -e
#SBATCH --error /home/td37cari/job.err

#here the jobscript starts


srun hostname

export OMPI_MCA_opal_warn_on_missing_libcuda=0
export OMPI_MCA_opal_common_ucx_opal_mem_hooks=1
export OMPI_MCA_osc=ucx
export OMPI_MCA_pml=ucx
export UCX_WARN_UNUSED_ENV_VARS=n
export UCX_UNIFIED_MODE=y

ml gcc/8.3.1 hwloc/2.5.0 openucx/1.12.0 openmpi/test

srun ./benchmark --iters $SLURM_ARRAY_TASK_ID --size 100 --count 10000 --threshold 20 --strategy 2 --data 0


#for strat in {0..3}
#do
#    for data in {0..2}
#    do
#        for iters in {100..5000..100}
#        do
#            for size in {100..100..10}
#            do
#                for count in {1000..1000..100}
#                do
#                    srun ./benchmark --iters $iters --size $size --count $count --threshold 20 --strategy $strat --data $data
#                done
#            done
#        done
#    done
#done
