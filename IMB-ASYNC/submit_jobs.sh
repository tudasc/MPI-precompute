#!/bin/bash

#small helper, that submits all 4 job arrays one for each communication mode

NC_PARAM="-n 2 -c 96"

export MODE=normal
# 0,1,2,3 for normal,rendevouz1,rendevouz2,eager
export MOD=0
sbatch $NC_PARAM -A $CUR_PROJ job_script.sh

export MODE=rendevouz1
# 0,1,2,3 for normal,rendevouz1,rendevouz2,eager
export MOD=1
sbatch $NC_PARAM -A $CUR_PROJ job_script.sh

export MODE=rendevouz2
# 0,1,2,3 for normal,rendevouz1,rendevouz2,eager
export MOD=2
sbatch $NC_PARAM -A $CUR_PROJ job_script.sh

export MODE=eager
# 0,1,2,3 for normal,rendevouz1,rendevouz2,eager
export MOD=3
sbatch $NC_PARAM -A $CUR_PROJ job_script.sh



