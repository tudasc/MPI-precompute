#include "initialization.h"
#include "globals.h"
#include "handshake.h"
#include "settings.h"

/// TODO clean up includes
#ifndef MPI_INTERNALS_INCLUDES
#define MPI_INTERNALS_INCLUDES
#include "ompi/mca/osc/base/base.h"
#include "ompi/mca/osc/base/osc_base_obj_convert.h"
#include "ompi/mca/osc/osc.h"
#include "opal/mca/common/ucx/common_ucx.h"

#include "ompi/mca/osc/ucx/osc_ucx.h"
#include "ompi/mca/osc/ucx/osc_ucx_request.h"
#endif // MPI_INTERNALS_INCLUDES

#include <stdlib.h>
#include <unistd.h>

void MPIOPT_INIT() {
  // create the global win used for rdma transfers
  // TODO maybe we need less initialization to initialize the RDMA component?
  MPI_Win_create(&dummy_int, sizeof(int), 1, MPI_INFO_NULL, MPI_COMM_WORLD,
                 &global_comm_win);
  request_list_head = malloc(sizeof(struct list_elem));
  request_list_head->elem = NULL;
  request_list_head->next = NULL;
  to_free_list_head = malloc(sizeof(struct list_elem));
  to_free_list_head->elem = NULL;
  to_free_list_head->next = NULL;

#ifdef SUMMARY_STATISTIC_PRINTING
  crosstalk_counter = 0;
#endif
  MPI_Comm_dup(MPI_COMM_WORLD, &handshake_communicator);
  MPI_Comm_dup(MPI_COMM_WORLD, &handshake_response_communicator);
#ifdef BUFFER_CONTENT_CHECKING
  MPI_Comm_dup(MPI_COMM_WORLD, &checking_communicator);
#endif
}

static int init_request(const void *buf, int count, MPI_Datatype datatype,
                        int dest, int tag, MPI_Comm comm,
                        MPIOPT_Request *request) {

  // TODO support other dtypes as MPI_BYTE
  assert(datatype == MPI_BYTE);
  assert(comm == MPI_COMM_WORLD); // currently only works for comm_wolrd
  int rank, numtasks;
  // Welchen rang habe ich?
  MPI_Comm_rank(comm, &rank);
  // wie viele Tasks gibt es?
  MPI_Comm_size(comm, &numtasks);

  uint64_t buffer_ptr = buf;

  ompi_osc_ucx_module_t *module =
      (ompi_osc_ucx_module_t *)global_comm_win->w_osc_module;
  ucp_ep_h ep = OSC_UCX_GET_EP(module->comm, dest);

  request->ep = ep;
  request->buf = buf;
  request->dest = dest;
  request->size = count;
  request->tag = tag;
  request->comm = comm;
  request->backup_request = MPI_REQUEST_NULL;
  request->remote_data_addr = NULL;

  request->operation_number = 0;
  request->flag = 2; // operation 0 is completed

#ifdef BUFFER_CONTENT_CHECKING
  // use c alloc, so that it is initialized, even if a smaller msg was received
  // to avoid undefined behaviour
  request->checking_buf = calloc(count, 1);
  request->chekcking_request = MPI_REQUEST_NULL;
#endif

  send_rdma_info(request);

  // add request to list, so that it is progressed, if other requests have to
  // wait
  add_request_to_list(request);

  return MPI_SUCCESS;
}

static int MPIOPT_Recv_init_internal(void *buf, int count,
                                     MPI_Datatype datatype, int source, int tag,
                                     MPI_Comm comm, MPIOPT_Request *request) {
#ifdef STATISTIC_PRINTING
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  printf("Rank %d: Init RECV from %d\n", rank, source);
#endif

  memset(request, 0, sizeof(MPIOPT_Request));
  request->type = RECV_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION;
  return init_request(buf, count, datatype, source, tag, comm, request);
}

static int MPIOPT_Send_init_internal(void *buf, int count,
                                     MPI_Datatype datatype, int source, int tag,
                                     MPI_Comm comm, MPIOPT_Request *request) {
#ifdef STATISTIC_PRINTING
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  printf("Rank %d: Init SEND to %d with msg size %d\n", rank, source, count);
#endif
  memset(request, 0, sizeof(MPIOPT_Request));
  request->type = SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION;
  return init_request(buf, count, datatype, source, tag, comm, request);
}
