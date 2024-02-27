# the modules
ml purge
ml gcc/8.5.0
ml llvm/16.0.1.RelAssert
ml cuda/11.8 hwloc/2.9.1

source /home/tj75qeje/mpi-comp-match/build/setup_env.sh


PARAM=$(awk "NR==$SLURM_ARRAY_TASK_ID" $VARIABLE_PARAM_FILE)
SPACELESS_PARAM=$(echo "$PARAM" | sed 's/ //g')

OUTFILE_ORIGINAL=ORIG_$SPACELESS_PARAM.csv
OUTFILE_ALTERED=ALTERED_$SPACELESS_PARAM.csv

TEMP_FILE=${SLURM_ARRAY_JOB_ID}_${SLURM_ARRAY_TASK_ID}

echo "original:"
srun $APPLICATION_ORIGINAL $PARAM $FIXED_PARAMS > $TEMP_FILE
val=$(read_output $TEMP_FILE)
echo "$SLURM_NTASKS,$val" >> $OUTFILE_ORIGINAL


echo "altered:"
srun $APPLICATION_ALTERED $PARAM $FIXED_PARAMS > $TEMP_FILE
val=$(read_output $TEMP_FILE)
echo "$SLURM_NTASKS,$val" >> $OUTFILE_ALTERED

rm $TEMP_FILE

