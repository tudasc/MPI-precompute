#ifndef MPIOPT_INIT_H_
#define MPIOPT_INIT_H_

#include "request_type.h"
#include "settings.h"
#include <mpi.h>

LINKAGE_TYPE int MPIOPT_Send_init_internal(const void *buf, int count,
                                           MPI_Datatype datatype, int source,
                                           int tag, MPI_Comm comm,
                                           MPIOPT_Request *request,
                                           MPI_Info info);
LINKAGE_TYPE int MPIOPT_Recv_init_internal(void *buf, int count,
                                           MPI_Datatype datatype, int source,
                                           int tag, MPI_Comm comm,
                                           MPIOPT_Request *request,
                                           MPI_Info info);

#endif /* MPIOPT_INIT_H_ */
