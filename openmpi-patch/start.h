#ifndef MPIOPT_START_H_
#define MPIOPT_START_H_

#include "request_type.h"
#include "settings.h"
#include <mpi.h>

LINKAGE_TYPE int b_send(MPIOPT_Request *request);
LINKAGE_TYPE int b_recv(MPIOPT_Request *request);

LINKAGE_TYPE int
start_send_when_searching_for_connection(MPIOPT_Request *request);
LINKAGE_TYPE int
start_recv_when_searching_for_connection(MPIOPT_Request *request);

LINKAGE_TYPE int start_send_fallback(MPIOPT_Request *request);
LINKAGE_TYPE int start_recv_fallback(MPIOPT_Request *request);

#endif /* MPIOPT_START_H_ */
