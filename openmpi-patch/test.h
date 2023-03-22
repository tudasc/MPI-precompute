#ifndef MPIOPT_TEST_H_
#define MPIOPT_TEST_H_

#include "request_type.h"
#include "settings.h"
#include <mpi.h>

LINKAGE_TYPE int test_send_request(MPIOPT_Request *request, int *flag,
                                   MPI_Status *status);
LINKAGE_TYPE int test_recv_request(MPIOPT_Request *request, int *flag,
                                   MPI_Status *status);
LINKAGE_TYPE int test_fallback(MPIOPT_Request *request, int *flag,
                               MPI_Status *status);
LINKAGE_TYPE int test_empty(MPIOPT_Request *request, int *flag,
                            MPI_Status *status);

LINKAGE_TYPE void progress_request(MPIOPT_Request *request);
LINKAGE_TYPE void progress_other_requests(MPIOPT_Request *current_request);

#endif /* MPIOPT_TEST_H_ */
