#ifndef MPIOPT_HANDSHAKE_H_
#define MPIOPT_HANDSHAKE_H_

#include "settings.h"

LINKAGE_TYPE void send_rdma_info(MPIOPT_Request *request);
LINKAGE_TYPE void receive_rdma_info(MPIOPT_Request *request);
LINKAGE_TYPE void
progress_recv_request_waiting_for_rdma(MPIOPT_Request *request);
LINKAGE_TYPE void
progress_send_request_waiting_for_rdma(MPIOPT_Request *request);

#endif /* MPIOPT_HANDSHAKE_H_ */
