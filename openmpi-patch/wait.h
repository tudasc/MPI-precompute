#ifndef MPIOPT_WAIT_H_
#define MPIOPT_WAIT_H_

#include "request_type.h"
#include <mpi.h>
LINKAGE_TYPE int MPIOPT_Wait_send_internal(MPIOPT_Request *request,
                                     MPI_Status *status);
LINKAGE_TYPE int MPIOPT_Wait_internal(MPIOPT_Request *request, MPI_Status *status);
LINKAGE_TYPE int MPIOPT_Wait_recv_internal(MPIOPT_Request *request,
                                     MPI_Status *status);

LINKAGE_TYPE void wait_for_completion_blocking(void *request);

#endif /* MPIOPT_WAIT_H_ */
