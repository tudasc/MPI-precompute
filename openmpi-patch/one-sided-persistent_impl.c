#include <mpi.h>
#include <ucp/api/ucp.h>

// why does other header miss this include?
#include <stdbool.h>

/// TODO clean up includes
#ifndef MPI_INTERNALS_INCLUDES
#define MPI_INTERNALS_INCLUDES
#include "ompi/mca/osc/base/base.h"
#include "ompi/mca/osc/base/osc_base_obj_convert.h"
#include "ompi/mca/osc/osc.h"
#include "opal/mca/common/ucx/common_ucx.h"
#include <stdio.h>
#include <time.h>

#include "ompi/mca/osc/ucx/osc_ucx.h"
#include "ompi/mca/osc/ucx/osc_ucx_request.h"
#endif // MPI_INTERNALS_INCLUDES

#include <stdlib.h>
#include <unistd.h>

#include "globals.h"
#include "request_type.h"
#include "settings.h"

#include "initialization.h"

int MPIOPT_Send_init(const void *buf, int count, MPI_Datatype datatype,
                     int dest, int tag, MPI_Comm comm, MPI_Request *request) {

  *request = malloc(sizeof(MPIOPT_Request));

  return MPIOPT_Send_init_internal(buf, count, datatype, dest, tag, comm,
                                   (MPIOPT_Request *)*request);
}

int MPIOPT_Recv_init(void *buf, int count, MPI_Datatype datatype, int source,
                     int tag, MPI_Comm comm, MPI_Request *request) {
  *request = malloc(sizeof(MPIOPT_Request));
  return MPIOPT_Recv_init_internal(buf, count, datatype, source, tag, comm,
                                   (MPIOPT_Request *)*request);
}

int MPIOPT_Start(MPI_Request *request) {
  return MPIOPT_Start_internal((MPIOPT_Request *)*request);
}

int MPIOPT_Start_send(MPI_Request *request) {
  return MPIOPT_Start_send_internal((MPIOPT_Request *)*request);
}

int MPIOPT_Start_recv(MPI_Request *request) {
  return MPIOPT_Start_recv_internal((MPIOPT_Request *)*request);
}

int MPIOPT_Wait_send(MPI_Request *request, MPI_Status *status) {
  return MPIOPT_Wait_send_internal((MPIOPT_Request *)*request, status);
}

int MPIOPT_Wait_recv(MPI_Request *request, MPI_Status *status) {
  return MPIOPT_Wait_recv_internal((MPIOPT_Request *)*request, status);
}

int MPIOPT_Wait(MPI_Request *request, MPI_Status *status) {
  return MPIOPT_Wait_internal((MPIOPT_Request *)*request, status);
}

int MPIOPT_Test(MPI_Request *request, int *flag, MPI_Status *status) {
  return MPIOPT_Test_internal((MPIOPT_Request *)*request, flag, status);
}

int MPIOPT_Waitany(int count, MPI_Request array_of_requests[], int *index,
                   MPI_Status *status) {

  int flag = 0;
  while (!flag) {
    for (int i = 0; i < count; ++i) {
      MPIOPT_Test(&array_of_requests[i], &flag, status);
      if (flag) {
        *index = i;
        return 0;
      }
    }
  }
  *index = MPI_UNDEFINED;
  return 0;
}

int MPIOPT_Testany(int count, MPI_Request array_of_requests[], int *index,
                   int *flag, MPI_Status *status) {

  for (int i = 0; i < count; ++i) {
    MPIOPT_Test(&array_of_requests[i], &flag, status);
    if (*flag) {
      *index = i;
      return 0;
    }
  }

  *index = MPI_UNDEFINED;
  return 0;
}

int MPIOPT_Waitall(int count, MPI_Request array_of_requests[],
                   MPI_Status array_of_statuses[]) {
  for (int i = 0; i < count; ++i) {
    MPIOPT_Wait(&array_of_requests[i], &array_of_statuses[i]);
  }
  return 0;
}

int MPIOPT_Testall(int count, MPI_Request array_of_requests[], int *flag,
                   MPI_Status array_of_statuses[]) {
  for (int i = 0; i < count; ++i) {
    MPIOPT_Test(&array_of_requests[i], flag, &array_of_statuses[i]);
    if (!flag)
      return 0; // found one request not complete
  }
  return 0;
}

int MPIOPT_Waitsome(int incount, MPI_Request array_of_requests[], int *outcount,
                    int array_of_indices[], MPI_Status array_of_statuses[]) {
  MPIOPT_Waitany(incount, array_of_requests, &array_of_indices[0],
                 array_of_statuses);
  *outcount = 1;
  return 0;
}

int MPIOPT_Testsome(int incount, MPI_Request array_of_requests[], int *outcount,
                    int array_of_indices[], MPI_Status array_of_statuses[]) {
  *outcount = 0;
  int flag = 0;
  for (int i = 0; i < incount; ++i) {
    MPIOPT_Test(&array_of_requests[i], &flag, &array_of_statuses[i]);
    if (flag) {
      array_of_indices[*outcount] = i;
      *outcount = *outcount + 1;
    }
    flag = 0;
  }

  return 0;
}

int MPIOPT_Request_free(MPI_Request *request) {
  int retval = MPIOPT_Request_free_internal((MPIOPT_Request *)*request);
  *request = NULL;
  // free(*request);
  return retval;
}
