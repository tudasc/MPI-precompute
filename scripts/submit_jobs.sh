#!/bin/bash

# builds the job_scipts and submits them for the given configuration script

if ! [ -f "$1" ]; then
echo "Error: Invalid configuration file"
exit -1
fi

config_file=$(realpath $1)
source $config_file

if ! [ -d "$EXPERIMENT_DIR" ]; then
echo "Error: Invalid configuration file"
exit -1
fi
#TODO one could add more checks about the given configuration file

#TODO read the number of processes from another config file

NPROC_ARRAY=("2" "4" "8" "16")

ARRAY_SIZE=$(wc -l $VARIABLE_PARAM_FILE | cut -d' ' -f1)

#build the job_script file

for NP in ${NPROC_ARRAY[@]}; do

JOB_SCRIPT=$EXPERIMENT_DIR/$NP.sh
  
cp SLURM_Header.sh $JOB_SCRIPT
echo "#SBATCH --ntasks $NP" >> $JOB_SCRIPT
echo "#SBATCH --job-name $APPLICATION_NAME" >> $JOB_SCRIPT
echo "#SBATCH --chdir $EXPERIMENT_DIR" >> $JOB_SCRIPT
echo "#SBATCH --output $EXPERIMENT_DIR/job_%A_%a.out" >> $JOB_SCRIPT
echo "#SBATCH --array 1-$ARRAY_SIZE" >> $JOB_SCRIPT
echo " " >> $JOB_SCRIPT
echo "source $config_file" >> $JOB_SCRIPT

cat job_script_body.sh >> $JOB_SCRIPT

# and submit
sbatch -A $CUR_PROJ $JOB_SCRIPT
done

