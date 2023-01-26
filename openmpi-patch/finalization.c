#include "finalization.h"
#include "globals.h"
#include "settings.h"

#include "mpi-internals.h"

#include <stdlib.h>
#include <unistd.h>

void MPIOPT_FINALIZE() {
  MPI_Win_free(&global_comm_win);
  assert(request_list_head->next == NULL); // list should be empty
  free(request_list_head);

#ifdef STATISTIC_PRINTING
  int rank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  printf("Rank %d: Finalize\n", rank);
#endif

  ucp_context_h context = mca_osc_ucx_component.ucp_context;

  struct list_elem *elem = to_free_list_head;
  while (elem != NULL) {
    MPIOPT_Request *req = elem->elem;

    if (req != NULL) {
      free(req->rdma_info_buf);
      // release all RDMA ressources

      if (req->type == RECV_REQUEST_TYPE || req->type == SEND_REQUEST_TYPE) {
        // otherwise all these resources where never acquired
        ucp_mem_unmap(context, req->mem_handle_flag);
        ucp_rkey_destroy(req->remote_flag_rkey);

        // ucp_mem_unmap(context, req->mem_handle_data); // was freed before
      }
      free(req);
    }
    struct list_elem *nxt_elem = elem->next;
    free(elem);
    elem = nxt_elem;
  }
#ifdef SUMMARY_STATISTIC_PRINTING
  printf("Crosstalk_counter= %d\n", crosstalk_counter);
#endif

  // TODO receive all pending messages from unsuccessful handshakes

  for (int i = 0; i < communicator_array_size; ++i) {
    MPI_Comm_free(&communicator_array[i].handshake_communicator);
    MPI_Comm_free(&communicator_array[i].handshake_response_communicator);
#ifdef BUFFER_CONTENT_CHECKING
    MPI_Comm_free(&communicator_array[i].checking_communicator);
#endif
  }
  free(communicator_array);
}

LINKAGE_TYPE int MPIOPT_Request_free_internal(MPIOPT_Request *request) {

#ifdef DISTINGUISH_ACTIVE_REQUESTS
  assert(request->active == 0);
#endif

#ifdef STATISTIC_PRINTING
  int rank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (request->type == SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION ||
      request->type == SEND_REQUEST_TYPE ||
      request->type == SEND_REQUEST_TYPE_USE_FALLBACK) {
    printf("Rank %d: SENDING: Request Free\n", rank);
  } else {
    printf("Rank %d: RECV: Request Free \n", rank);
  }
#endif
  remove_request_from_list(request);

  // cancel any search for RDMA connection, if necessary

  // defer free of memory until finalize, as the other process may start an RDMA
  // communication ON THE FLAG, not on the data which may lead to error, if we
  // free the mem beforehand but we can unmap the data part, as the oter process
  // will not rdma to it
  if (request->type == RECV_REQUEST_TYPE ||
      request->type == SEND_REQUEST_TYPE) {

    assert(request->ucx_request_data_transfer == NULL);
    assert(request->ucx_request_flag_transfer == NULL);
    ucp_context_h context = mca_osc_ucx_component.ucp_context;
    // ucp_mem_unmap(context, request->mem_handle_flag);// deferred
    ucp_mem_unmap(context, request->mem_handle_data);
    ucp_rkey_destroy(request->remote_data_rkey);
  }

  struct list_elem *new_elem = malloc(sizeof(struct list_elem));
  new_elem->elem = request;
  new_elem->next = to_free_list_head->next;
  to_free_list_head->next = new_elem;

  /*
   free(request->rdma_info_buf);

   if (request->type == RECV_REQUEST_TYPE ||
   request->type == SEND_REQUEST_TYPE) {
   // otherwise all these ressources where never aquired

   acknowlege_Request_free(request);
   }

   request->type = 0; // uninitialized
   */
#ifdef BUFFER_CONTENT_CHECKING
  free(request->checking_buf);
#endif

  return MPI_SUCCESS;
}