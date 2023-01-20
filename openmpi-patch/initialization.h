#ifndef MPIOPT_INIT_H_
#define MPIOPT_INIT_H_

#include "request_type.h"
#include <mpi.h>

static int MPIOPT_Send_init_internal(void *buf, int count,
                                     MPI_Datatype datatype, int source, int tag,
                                     MPI_Comm comm, MPIOPT_Request *request);
static int MPIOPT_Recv_init_internal(void *buf, int count,
                                     MPI_Datatype datatype, int source, int tag,
                                     MPI_Comm comm, MPIOPT_Request *request);

#endif /* MPIOPT_INIT_H_ */
