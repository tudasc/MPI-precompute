#!/bin/bash

# set during script generation
#### same as -n
####SBATCH --ntasks 2

#SBATCH --mem-per-cpu=3800
#same as -t
#SBATCH --time 00:30:00


#same as -c
#SBATCH --cpus-per-task 104
# one node per process
