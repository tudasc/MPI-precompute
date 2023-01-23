#ifndef MPIOPT_MPI_INTERNALS_H_
#define MPIOPT_MPI_INTERNALS_H_

// inclusion of some openmpi internal headers for some functionality we use
// some dont have an include guard, therefore we bundle them up all together
// here

// TODO get rid of the implicit function definition of asprintf warning
#define _GNU_SOURCE
#define __STDC_WANT_LIB_EXT2 1
#include <stdio.h>

#include "ompi/mca/osc/base/base.h"
#include "ompi/mca/osc/base/osc_base_obj_convert.h"
#include "ompi/mca/osc/osc.h"
#include "opal/mca/common/ucx/common_ucx.h"

#include "ompi/mca/osc/ucx/osc_ucx.h"
#include "ompi/mca/osc/ucx/osc_ucx_request.h"

#endif /* MPIOPT_MPI_INTERNALS_H_ */
