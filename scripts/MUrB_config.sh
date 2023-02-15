#!/bin/bash


export APPLICATION_NAME=MUrB

export EXPERIMENT_DIR=/work/scratch/tj75qeje/mpi-comp-match/MUrB

export APPLICATION_ORIGINAL=/home/tj75qeje/mpi-comp-match_sample_codes/MUrB_original/build/bin/murb
export APPLICATION_ALTERED=/home/tj75qeje/mpi-comp-match_sample_codes/MUrB/build/bin/murb

export VARIABLE_PARAM_FILE=/home/tj75qeje/mpi-comp-match/scripts/parameters_MUrB.txt

export FIXED_PARAMS="-v --im 100"


function read_output {
  grep "Entire simulation took" $1 | cut -d ' ' -f 4
}
