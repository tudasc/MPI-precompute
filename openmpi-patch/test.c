#include "test.h"
#include "globals.h"
#include "settings.h"

#include "handshake.h"
#include "wait.h"

#include "mpi-internals.h"

#include <stdlib.h>
#include <unistd.h>

static void empty_function_in_test_c(void *request, ucs_status_t status) {
  // callback if flush is completed
}

LINKAGE_TYPE void progress_send_request(MPIOPT_Request *request) {
  assert(request->type == SEND_REQUEST_TYPE);
  // progress
  ucp_worker_progress(mca_osc_ucx_component.ucp_worker);

  // and check for completion
  if (__builtin_expect(request->ucx_request_flag_transfer != NULL, 0)) {
    if (ucp_request_check_status(request->ucx_request_flag_transfer) !=
        UCS_INPROGRESS) {
      ucp_request_free(request->ucx_request_flag_transfer);
      request->ucx_request_flag_transfer = NULL;
    }
  }
  if (__builtin_expect(request->ucx_request_data_transfer != NULL, 0)) {
    if (ucp_request_check_status(request->ucx_request_data_transfer) !=
        UCS_INPROGRESS) {
      ;
      ucp_request_free(request->ucx_request_data_transfer);
      request->ucx_request_data_transfer = NULL;
    }
  }
}

LINKAGE_TYPE void progress_recv_request(MPIOPT_Request *request) {
  assert(request->type == RECV_REQUEST_TYPE);
  // check for crosstalk
  if (__builtin_expect(request->flag == request->operation_number * 2 + 1, 0)) {
    assert(request->ucx_request_data_transfer == NULL);
    if (request->ucx_request_flag_transfer != NULL) {
      wait_for_completion_blocking(request->ucx_request_flag_transfer);
      request->ucx_request_flag_transfer = NULL;
    }
    // only then the sender is ready, but the recv not started yet
    request->flag++; // recv is done at our side
    // no possibility of data race, WE will advance the comm
    assert(request->flag == request->operation_number * 2 + 2);
#ifdef STATISTIC_PRINTING
    printf("crosstalk detected\n");
    printf("recv fetches data\n");
#endif
    ucs_status_t status;
    if(request->is_cont){
      status =
          ucp_get_nbi(request->ep, (void *)request->buf, request->size,
                      request->remote_data_addr, request->remote_data_rkey);
    } else {
      status =
          ucp_get_nbi(request->ep, (void *)request->packed_buf, request->size,
                      request->remote_data_addr, request->remote_data_rkey);
    }

    assert(status == UCS_OK || status == UCS_INPROGRESS);
    /*
     if (status != UCS_OK && status != UCS_INPROGRESS) {
     printf("ERROR in RDMA GET\n");
     }*/
    // ensure order:
    status = ucp_worker_fence(mca_osc_ucx_component.ucp_worker);
    assert(status == UCS_OK || status == UCS_INPROGRESS);

    request->flag_buffer = request->operation_number * 2 + 2;
    status = ucp_put_nbi(request->ep, &request->flag_buffer, sizeof(int),
                         request->remote_flag_addr, request->remote_flag_rkey);
    assert(status == UCS_OK || status == UCS_INPROGRESS);

    request->ucx_request_data_transfer =
        ucp_ep_flush_nb(request->ep, 0, empty_function_in_test_c);
#ifdef DISTORT_PROCESS_ORDER_ON_CROSSTALK
    // distort process order, so that crosstalk is unlikely to happen again
    // the larger the msg, the more important that processes are apart and no
    // crosstalk takes place
    usleep(rand() % (request->size));
#endif
#ifdef SUMMARY_STATISTIC_PRINTING
    crosstalk_counter++;
#endif
  } // end crosstalk check

  // and progress all communication regardless if we need to initiate something
  ucp_worker_progress(mca_osc_ucx_component.ucp_worker);

  // check for completion
  if (__builtin_expect(request->ucx_request_flag_transfer != NULL, 0)) {
    if (ucp_request_check_status(request->ucx_request_flag_transfer) !=
        UCS_INPROGRESS) {
      ucp_request_free(request->ucx_request_flag_transfer);
      request->ucx_request_flag_transfer = NULL;
    }
  }
  if (__builtin_expect(request->ucx_request_data_transfer != NULL, 0)) {
    if (ucp_request_check_status(request->ucx_request_data_transfer) !=
        UCS_INPROGRESS) {
      ucp_request_free(request->ucx_request_data_transfer);
      request->ucx_request_data_transfer = NULL;
    }
  }
}

LINKAGE_TYPE void progress_request(MPIOPT_Request *request) {
  if (request->type == SEND_REQUEST_TYPE) {
    progress_send_request(request);
  } else if (request->type == RECV_REQUEST_TYPE) {
    progress_recv_request(request);
  } else if (request->type == SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION) {
    progress_send_request_waiting_for_rdma(request);
  } else if (request->type == RECV_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION) {
    progress_recv_request_waiting_for_rdma(request);
  } else if (request->type == SEND_REQUEST_TYPE_USE_FALLBACK) {
    int flag;
    // progress the fallback communication
    MPI_Test(&request->backup_request, &flag, MPI_STATUSES_IGNORE);
  } else if (request->type == RECV_REQUEST_TYPE_USE_FALLBACK) {
    int flag;
    // progress the fallback communication
    MPI_Test(&request->backup_request, &flag, MPI_STATUSES_IGNORE);
  } else {
    assert((request->type != RECV_REQUEST_TYPE_NULL ||
            request->type != SEND_REQUEST_TYPE_NULL) &&
           "Error: uninitialized Request");
  }
}

// call if one get stuck while waiting for a request to complete: progresses all
// other requests
LINKAGE_TYPE void progress_other_requests(MPIOPT_Request *current_request) {
  struct list_elem *current_elem = request_list_head->next;

  while (current_elem != NULL) {
    // we are stuck on this request, and should progress the others
    // after we return, the control flow goes back to this request anyway
    if (current_elem->elem != current_request) {
      progress_request(current_elem->elem);
    }
    current_elem = current_elem->next;
  }
}

LINKAGE_TYPE int MPIOPT_Test_internal(MPIOPT_Request *request, int *flag,
                                      MPI_Status *status) {

  if (__builtin_expect(status != MPI_STATUS_IGNORE, 0)) {
    status->MPI_TAG = request->tag;
    status->MPI_SOURCE = request->dest;
    status->MPI_ERROR = MPI_SUCCESS;
  }

#ifdef DISTINGUISH_ACTIVE_REQUESTS
  if (request->active == 0) {
    *flag = 1;
    return MPI_SUCCESS;
  }
#endif

  int ret_status = 0;
  if (request->type == SEND_REQUEST_TYPE_USE_FALLBACK ||
      request->type == RECV_REQUEST_TYPE_USE_FALLBACK) {
    ret_status = MPI_Test(&request->backup_request, flag, status);
#ifdef DISTINGUISH_ACTIVE_REQUESTS
    if (*flag)
      request->active = 0;
#endif
  } else {
    progress_request(request);
    // it is possible, that the other rank already started the next operation,
    // therefore
    // >=
    if (request->flag >= request->operation_number * 2 + 2 &&
        request->ucx_request_flag_transfer == NULL &&
        request->ucx_request_data_transfer == NULL) {
      // request is finished
      *flag = 1;
#ifdef DISTINGUISH_ACTIVE_REQUESTS
      request->active = 0;
#endif
    } else
      *flag = 0;
  }
#ifdef BUFFER_CONTENT_CHECKING
  if (*flag == 1) {
    // TODO buffer checking will break if the user tests a finished request
    assert(request->chekcking_request != MPI_REQUEST_NULL);
    MPI_Wait(&request->chekcking_request, MPI_STATUS_IGNORE);
    if (request->type == RECV_REQUEST_TYPE ||
        request->type == RECV_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION ||
        request->type == RECV_REQUEST_TYPE_USE_FALLBACK) {
      int buffer_has_expected_content =
          memcmp(request->checking_buf, request->buf, request->size);
      assert(buffer_has_expected_content == 0 &&
             "Error, The buffer has not the content of the message");
    }
  }
#endif

  return ret_status;
}