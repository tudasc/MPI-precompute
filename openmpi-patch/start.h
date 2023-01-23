#ifndef MPIOPT_START_H_
#define MPIOPT_START_H_

#include "request_type.h"
#include "settings.h"
#include <mpi.h>

LINKAGE_TYPE int MPIOPT_Start_send_internal(MPIOPT_Request *request);
LINKAGE_TYPE int MPIOPT_Start_recv_internal(MPIOPT_Request *request);
LINKAGE_TYPE int MPIOPT_Start_internal(MPIOPT_Request *request);

#endif /* MPIOPT_START_H_ */
