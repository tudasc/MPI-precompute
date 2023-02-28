#!/bin/bash


export APPLICATION_NAME=MUrB

export EXPERIMENT_DIR=/work/scratch/tj75qeje/mpi-comp-match/MUrB

export APPLICATION_ORIGINAL=/home/tj75qeje/mpi-comp-match_sample_codes/MUrB_original/build/bin/murb
export APPLICATION_ALTERED=/home/tj75qeje/mpi-comp-match_sample_codes/MUrB/build/bin/murb

export VARIABLE_PARAM_FILE=/home/tj75qeje/mpi-comp-match/scripts/MUrB/parameters_MUrB.txt

export FIXED_PARAMS="-v --im 100"


function read_output {
  runtime=$(grep "Entire simulation took" $1 | cut -d ' ' -f 4)
  num_p=$(grep "nb. of MPI procs" $1 | cut -d ':' -f 2)
  num_t=$(grep "nb. of threads" $1 | cut -d ':' -f 2)
  b_p_proc=$(grep "nb. of bodies per proc" $1 | cut -d ':' -f 2)  
  mem=$(grep "mem. allocated" $1 | cut -d ':' -f 2)  
  echo "$runtime,$num_p,$num_t,$b_p_proc,$mem"
}
