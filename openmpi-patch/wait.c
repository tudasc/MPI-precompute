#include "wait.h"
#include "globals.h"
#include "handshake.h"
#include "settings.h"

#include "test.h"

#include "mpi-internals.h"

#include <stdlib.h>
#include <unistd.h>

LINKAGE_TYPE void wait_for_completion_blocking(void *request) {
  assert(request != NULL);
  ucs_status_t status;
  do {
    ucp_worker_progress(mca_osc_ucx_component.ucp_worker);
    status = ucp_request_check_status(request);
  } while (status == UCS_INPROGRESS);
  ucp_request_free(request);
}

// operation_number*2= op has not started on remote
// operation_number*2 +1= op has started on remote, we should initiate
// data-transfer operation_number*2 + 2= op has finished on remote

LINKAGE_TYPE void e_send(MPIOPT_Request *request) {

  while (__builtin_expect(request->ucx_request_data_transfer != NULL ||
                              request->ucx_request_flag_transfer != NULL,
                          0)) {
    progress_send_request(request);
  }
  // we need to wait until the op has finished on the remote before re-using the
  // data buffer
  int count = 0;
  // busy wait
  while (__builtin_expect(request->flag < request->operation_number * 2 + 2 &&
                              count < RDMA_SPIN_WAIT_THRESHOLD,
                          0)) {
    ++count;
    ucp_worker_progress(mca_osc_ucx_component.ucp_worker);
  }

  while (
      __builtin_expect(request->flag < request->operation_number * 2 + 2, 0)) {
    progress_other_requests(request);
    ucp_worker_progress(mca_osc_ucx_component.ucp_worker);
    // after some time: also test if the other rank has freed the request in
    // between
    // e_send_with_comm_abort_test(request);
    // TODO one could implement this and use fallback option
  }

  assert(request->flag >= request->operation_number * 2 + 2);
}

LINKAGE_TYPE void e_recv(MPIOPT_Request *request) {
  // ucp_worker_progress(mca_osc_ucx_component.ucp_worker);

  progress_recv_request(request); // will detect crosstalk if present
  // therefore we need one progress call if no requests are present
  while (__builtin_expect(request->ucx_request_data_transfer != NULL ||
                              request->ucx_request_flag_transfer != NULL,
                          0)) {
    progress_recv_request(request);
  }

  int count = 0;
  // busy wait
  while (__builtin_expect(request->flag < request->operation_number * 2 + 2 &&
                              count < RDMA_SPIN_WAIT_THRESHOLD,
                          0)) {
    ++count;
    ucp_worker_progress(mca_osc_ucx_component.ucp_worker);
  }

  while (
      __builtin_expect(request->flag < request->operation_number * 2 + 2, 0)) {
    progress_other_requests(request);
    ucp_worker_progress(mca_osc_ucx_component.ucp_worker);
    // e_recv_with_comm_abort_test(request);
    // TODO one could implement this and use fallback if necessary
  }

  assert(request->flag >= request->operation_number * 2 + 2);
  assert(request->ucx_request_data_transfer == NULL &&
         request->ucx_request_flag_transfer == NULL);
}

// TODO return proper error codes

LINKAGE_TYPE void
wait_send_when_searching_for_connection(MPIOPT_Request *request) {

  int flag = 0;

  assert(request->operation_number == 1);

  while (!flag) {
    progress_send_request_waiting_for_rdma(request);
    if (request->type != SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION)
      flag = 1; // done
    progress_other_requests(request);
  }

  assert(request->type != SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION);
}

LINKAGE_TYPE void
wait_recv_when_searching_for_connection(MPIOPT_Request *request) {

  assert(request->operation_number == 1);

  while (request->type == RECV_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION) {
    progress_other_requests(request);
    progress_recv_request_waiting_for_rdma(request);
  }
  assert(request->type != RECV_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION);
}

LINKAGE_TYPE int MPIOPT_Wait_send_internal(MPIOPT_Request *request,
                                           MPI_Status *status) {

  // TODO implement MPI status?
  assert(status == MPI_STATUS_IGNORE);

  if (__builtin_expect(request->type == SEND_REQUEST_TYPE, 1)) {
    e_send(request);
  } else if (request->type == SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION) {
    wait_send_when_searching_for_connection(request);
  } else if (request->type == SEND_REQUEST_TYPE_USE_FALLBACK) {
    MPI_Wait(&request->backup_request, status);
  } else {
    assert(false && "Error: uninitialized Request");
  }
}

LINKAGE_TYPE int MPIOPT_Wait_recv_internal(MPIOPT_Request *request,
                                           MPI_Status *status) {

  // TODO implement MPI status?
  assert(status == MPI_STATUS_IGNORE);

  if (__builtin_expect(request->type == RECV_REQUEST_TYPE, 1)) {
    e_recv(request);
  } else if (request->type == RECV_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION) {
    wait_recv_when_searching_for_connection(request);
  } else if (request->type == RECV_REQUEST_TYPE_USE_FALLBACK) {

    MPI_Wait(&request->backup_request, status);
  } else {
    assert(false && "Error: uninitialized Request");
  }
}

LINKAGE_TYPE int MPIOPT_Wait_internal(MPIOPT_Request *request,
                                      MPI_Status *status) {
  int ret_status = 0;
  if (request->type == SEND_REQUEST_TYPE ||
      request->type == SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION ||
      request->type == SEND_REQUEST_TYPE_USE_FALLBACK) {
    ret_status = MPIOPT_Wait_send_internal(request, status);
  } else {
    int ret_status = MPIOPT_Wait_recv_internal(request, status);
  }

#ifdef BUFFER_CONTENT_CHECKING
  assert(request->chekcking_request != MPI_REQUEST_NULL);
  MPI_Wait(&request->chekcking_request, MPI_STATUS_IGNORE);
  if (request->type == SEND_REQUEST_TYPE ||
      request->type == SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION ||
      request->type == SEND_REQUEST_TYPE_USE_FALLBACK) {
    int buffer_has_expected_content =
        memcmp(request->checking_buf, request->buf, request->size);
    assert(buffer_has_expected_content == 0 &&
           "Error, The buffer has not the content of the message");
  }
#endif
  return ret_status;

  if (__builtin_expect(status != MPI_STATUS_IGNORE, 0)) {
    status->MPI_TAG = request->tag;
    status->MPI_SOURCE = request->dest;
    status->MPI_ERROR = MPI_SUCCESS;
  }
}