#ifndef MPIOPT_FINISH_H_
#define MPIOPT_FINISH_H_

#include "request_type.h"
#include "settings.h"
#include <mpi.h>

LINKAGE_TYPE int MPIOPT_Request_free_internal(MPIOPT_Request *request);

#endif /* MPIOPT_FINISH_H_ */