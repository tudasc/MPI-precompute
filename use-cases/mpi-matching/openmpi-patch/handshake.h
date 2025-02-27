#ifndef MPIOPT_HANDSHAKE_H_
#define MPIOPT_HANDSHAKE_H_

#include "request_type.h"
#include "settings.h"

LINKAGE_TYPE void send_rdma_info(MPIOPT_Request *request);
LINKAGE_TYPE void receive_rdma_info(MPIOPT_Request *request);

LINKAGE_TYPE int progress_send_request_handshake_begin(MPIOPT_Request *request,
                                                       int *flag,
                                                       MPI_Status *status);

LINKAGE_TYPE int progress_recv_request_handshake_begin(MPIOPT_Request *request,
                                                       int *flag,
                                                       MPI_Status *status);

#endif /* MPIOPT_HANDSHAKE_H_ */
