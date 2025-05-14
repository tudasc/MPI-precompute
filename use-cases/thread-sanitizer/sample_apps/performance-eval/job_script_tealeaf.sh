#!/bin/bash

# same as -n
#SBATCH --ntasks 1

#SBATCH --mem-per-cpu=3800
#same as -t
#SBATCH --time 00:30:00
#SBATCH --exclusive

#SBATCH --array 1-6


#same as -c
#SBATCH --cpus-per-task 8

#change for debugging the environment
#SBATCH -o /dev/null
#SBATCH -e /dev/null

#specify these variables
PRECOMPUTE_DIR="/home/tj75qeje/precompute/build/use-cases/thread-sanitizer"
EXECUTABLE_DIR="/home/tj75qeje/precompute/use-cases/thread-sanitizer/sample_apps/performance-eval/TEALEAF"
PARAMETER_FILE="/home/tj75qeje/precompute/use-cases/thread-sanitizer/sample_apps/performance-eval/parameters_tealeaf.txt"
OUTPUT_DIR="/work/scratch/tj75qeje/precompute/tealeaf"


OUTPUT_FILE_PREFIX="$OUTPUT_DIR/${SLURM_ARRAY_JOB_ID}_${SLURM_ARRAY_TASK_ID}_output"
RUNDIR="$OUTPUT_DIR/${SLURM_ARRAY_JOB_ID}_${SLURM_ARRAY_TASK_ID}"
mkdir -p $RUNDIR
cd $RUNDIR

ml gcc/8.5.0 clang/16.0.6
source ${PRECOMPUTE_DIR}/setup_env.sh
export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK
export OMP_PLACES=cores

# read from parameter file
RUN_PARAMETER=$(sed -n "${SLURM_ARRAY_TASK_ID}p" $PARAMETER_FILE)
# Extract values from CONFIG (split at ,)
IFS=',' read -r RESOLUTION STEPS <<< "$RUN_PARAMETER"

# setup tea.in
cp $EXECUTABLE_DIR/tea.in tea.in
cp $EXECUTABLE_DIR/tea.problems tea.problems
# update input file with correct parameters
sed -i \
    -e "s/^x_cells=.*/x_cells=$RESOLUTION/" \
    -e "s/^y_cells=.*/y_cells=$RESOLUTION/" \
    -e "s/^end_step=.*/end_step=$STEPS/" \
	tea.in


/usr/bin/time --format "$RUN_PARAMETER,$SLURM_CPUS_PER_TASK,without,%es" --output=${OUTPUT_FILE_PREFIX}_without ${EXECUTABLE_DIR}/without.exe $RUN_PARAMETER

/usr/bin/time --format "$RUN_PARAMETER,$SLURM_CPUS_PER_TASK,normal,%es" --output=${OUTPUT_FILE_PREFIX}_normal ${EXECUTABLE_DIR}/normal.exe $RUN_PARAMETER

/usr/bin/time --format "$RUN_PARAMETER,$SLURM_CPUS_PER_TASK,modified,%es" --output=${OUTPUT_FILE_PREFIX}_modified ${EXECUTABLE_DIR}/modified.exe $RUN_PARAMETER




