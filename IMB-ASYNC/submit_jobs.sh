#!/bin/bash

#small helper, that submits all 4 job arrays one for each communication mode

#N_PARAM="-n 384 --tasks-per-node 96"
N_PARAM="-n 192 --tasks-per-node 96"
#N_PARAM="-n 2 --tasks-per-node 1 --exclusive"

export MODE=normal
# 0,1,2,3 for normal,rendevouz1,rendevouz2,eager
export MOD=0
sbatch $N_PARAM -A $CUR_PROJ job_script.sh

export MODE=rendevouz1
# 0,1,2,3 for normal,rendevouz1,rendevouz2,eager
export MOD=1
sbatch $N_PARAM -A $CUR_PROJ job_script.sh

export MODE=rendevouz2
# 0,1,2,3 for normal,rendevouz1,rendevouz2,eager
export MOD=2
sbatch $N_PARAM -A $CUR_PROJ job_script.sh

export MODE=eager
# 0,1,2,3 for normal,rendevouz1,rendevouz2,eager
export MOD=3
sbatch $N_PARAM -A $CUR_PROJ job_script.sh



