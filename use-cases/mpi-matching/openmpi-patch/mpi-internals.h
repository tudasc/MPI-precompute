#ifndef MPIOPT_MPI_INTERNALS_H_
#define MPIOPT_MPI_INTERNALS_H_

// inclusion of some openmpi internal headers for some functionality we use
// some dont have an include guard, therefore we bundle them up all together
// here

#ifdef _STDIO_H
// we need to include stdio.h after the feature macro definition
// the internals use the function asprintf somewere
_Static_assert(0, "Wrong include ordering, include mpi-internals.h first");
#endif

#define _GNU_SOURCE
#define __STDC_WANT_LIB_EXT2 1
#include <stdio.h>

#include "ompi/mca/osc/base/base.h"
#include "ompi/mca/osc/base/osc_base_obj_convert.h"
#include "ompi/mca/osc/osc.h"
#include "opal/mca/common/ucx/common_ucx.h"

#include "ompi/mca/osc/ucx/osc_ucx.h"
#include "ompi/mca/osc/ucx/osc_ucx_request.h"

#define MPIOPT_REQUEST_TYPE (OMPI_REQUEST_MAX + 1)

#endif /* MPIOPT_MPI_INTERNALS_H_ */
