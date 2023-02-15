# the modules
ml purge
ml gcc/8.3.1
ml hwloc/2.5.0 clang/11.1.0
ml openucx/1.12.0

ml openmpi/rendevouz2

#export OMPI_MCA_opal_warn_on_missing_libcuda=0
#export OMPI_MCA_opal_common_ucx_opal_mem_hooks=1
export OMPI_MCA_osc=ucx
export OMPI_MCA_pml=ucx
export UCX_WARN_UNUSED_ENV_VARS=n
export UCX_UNIFIED_MODE=y


PARAM=$(awk "NR==$SLURM_ARRAY_TASK_ID" $VARIABLE_PARAM_FILE)
SPACELESS_PARAM=$(echo $PARAM | sed 's/ //g')

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

