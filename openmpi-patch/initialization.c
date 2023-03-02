#include "initialization.h"
#include "globals.h"
#include "handshake.h"
#include "interface.h"
#include "request_type.h"
#include "settings.h"

#include "debug.h"
#include "mpi-internals.h"
#include <stdlib.h>
#include <unistd.h>

// dummy int to create some mpi win on
int dummy_int = 0;

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

  communicator_array =
      malloc(sizeof(struct communicator_info) * MAX_NUM_OF_COMMUNICATORS);

#ifdef SUMMARY_STATISTIC_PRINTING
  crosstalk_counter = 0;
#endif
  MPIOPT_Register_Communicator(MPI_COMM_WORLD);
}

struct communicator_info *find_comm(MPI_Comm comm) {
  for (int i = 0; i < communicator_array_size; ++i) {
    if (communicator_array[i].original_communicator == comm)
      return &communicator_array[i];
  }

#ifdef REGISTER_COMMUNICATOR_ON_USE
  MPIOPT_Register_Communicator(comm);
  for (int i = 0; i < communicator_array_size; ++i) {
    if (communicator_array[i].original_communicator == comm)
      return &communicator_array[i];
  }
#endif

  assert(false && "Communicator was not registered");
  return NULL;
}

#ifdef CHECK_FOR_MATCHING_CONFLICTS
LINKAGE_TYPE int check_for_conflicting_request(MPIOPT_Request *request) {

  struct list_elem *current = request_list_head->next;
  while (current != NULL) {
    MPIOPT_Request *other = current->elem;
    assert(other != NULL);
    if (other != request) {
      // same communication direction
      if ((is_sending_type(request) && is_sending_type(other)) ||
          (is_recv_type(request) && is_recv_type(other))) {
        // same envelope
        if (request->dest == other->dest && request->tag == other->tag &&
            request->communicators->original_communicator ==
                other->communicators->original_communicator) {
          assert(false &&
                 "Requests with a matching envelope are not permitted");
          return 1;
        }
      }
    }
    current = current->next;
  }
  return 0;
}
#endif

LINKAGE_TYPE int init_request(const void *buf, int count, MPI_Datatype datatype,
                              int dest, int tag, MPI_Comm comm,
                              MPIOPT_Request *request, bool is_send_request) {

  MPI_Count type_size, type_extend, lb;
  MPI_Type_size_x(datatype, &type_size);
#ifndef NDEBUG
  // only if assertion checking is on
  MPI_Type_get_extent_x(datatype, &lb, &type_extend);
#endif
  // is contigous
  assert(type_size == type_extend && lb == 0 &&
         "Only contigous datatypes are supported yet");
  assert(type_size != MPI_UNDEFINED);

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
  request->size = type_size * count;
  request->tag = tag;
  request->backup_request = MPI_REQUEST_NULL;
  request->remote_data_addr = NULL;

  request->communicators = find_comm(comm);
  assert(request->communicators != NULL);

  request->operation_number = 0;
  request->flag = 2; // operation 0 is completed

#ifndef NDEBUG
  init_debug_data(request);
#endif

#ifdef BUFFER_CONTENT_CHECKING
  // use c alloc, so that it is initialized, even if a smaller msg was received
  // to avoid undefined behaviour
  request->checking_buf = calloc(request->size, 1);
  request->chekcking_request = MPI_REQUEST_NULL;
#endif

  int conflicts = 0;
#ifdef CHECK_FOR_MATCHING_CONFLICTS
  conflicts = check_for_conflicting_request(request);
#endif

#ifdef USE_FALLBACK_UNTIL_THRESHOLD
  const bool use_fallback = request->size < FALLBACK_THRESHOLD;
#ifdef SUMMARY_STATISTIC_PRINTING
  if (use_fallback)
    printf("Use Fallback for small msg\n");
#endif
#else
  const bool use_fallback = 0;
#endif

  if (use_fallback || rank == dest || rank == MPI_PROC_NULL || conflicts) {
    // use the default implementation for communication with self / no-op
    if (!is_send_request) {
      set_request_type(request, RECV_REQUEST_TYPE_USE_FALLBACK);
    } else {
      set_request_type(request, SEND_REQUEST_TYPE_USE_FALLBACK);
    }
  } else {
    if (!is_send_request) {
      set_request_type(request, RECV_REQUEST_TYPE_HANDSHAKE_NOT_STARTED);
    } else {
      set_request_type(request, SEND_REQUEST_TYPE_HANDSHAKE_NOT_STARTED);
    }
  }
  // add request to list, so that it is progressed, if other requests have to
  // wait
  add_request_to_list(request);

  return MPI_SUCCESS;
}

LINKAGE_TYPE int MPIOPT_Recv_init_internal(void *buf, int count,
                                           MPI_Datatype datatype, int source,
                                           int tag, MPI_Comm comm,
                                           MPIOPT_Request *request) {
  memset(request, 0, sizeof(MPIOPT_Request));
  return init_request(buf, count, datatype, source, tag, comm, request, false);
}

LINKAGE_TYPE int MPIOPT_Send_init_internal(void *buf, int count,
                                           MPI_Datatype datatype, int source,
                                           int tag, MPI_Comm comm,
                                           MPIOPT_Request *request) {
  memset(request, 0, sizeof(MPIOPT_Request));
  return init_request(buf, count, datatype, source, tag, comm, request, true);
}
