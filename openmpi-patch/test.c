#include "request_type.h"

#include "globals.h"
#include "settings.h"
#include "test.h"

#include "handshake.h"
#include "wait.h"
#include "pack.h"

#include "debug.h"
#include "mpi-internals.h"

#include <stdlib.h>
#include <unistd.h>

static void empty_function_in_test_c(void *request, ucs_status_t status) {
  // callback if flush is completed
}

inline static void set_mpi_status(MPIOPT_Request *request, MPI_Status *status) {
  if (__builtin_expect(status != MPI_STATUS_IGNORE, 0)) {
    status->MPI_TAG = request->tag;
    status->MPI_SOURCE = request->dest;
    status->MPI_ERROR = MPI_SUCCESS;
  }
}

LINKAGE_TYPE int test_send_request(MPIOPT_Request *request, int *flag,
                                   MPI_Status *status) {
  *flag = 0;
  assert(request->type == SEND_REQUEST_TYPE);
#ifndef NDEBUG
  add_operation_to_trace(request, "Progress_Request");
#endif
#ifdef DISTINGUISH_ACTIVE_REQUESTS
  if (request->active == 0) {
    *flag = 1;
    return MPI_SUCCESS;
  }
#endif

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

  if (__builtin_expect(request->flag >= request->operation_number * 2 + 2 &&
                           request->ucx_request_flag_transfer == NULL &&
                           request->ucx_request_data_transfer == NULL,
                       1)) {
    // request is finished
    *flag = 1;
#ifdef DISTINGUISH_ACTIVE_REQUESTS
    request->active = 0;
#endif
    set_mpi_status(request, status);
  } else {
    *flag = 0;
  }
  return MPI_SUCCESS;
}

LINKAGE_TYPE int test_recv_request(MPIOPT_Request *request, int *flag,
                                   MPI_Status *status) {
  *flag = 0;
  assert(request->type == RECV_REQUEST_TYPE);
#ifndef NDEBUG
  add_operation_to_trace(request, "Progress_Request");
#endif
#ifdef DISTINGUISH_ACTIVE_REQUESTS
  if (request->active == 0) {
    *flag = 1;
    return MPI_SUCCESS;
  }
#endif
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
#ifndef NDEBUG
    add_operation_to_trace(request, "CROSSTALK DETECTED");
    add_operation_to_trace(request, "recv fetches data");
#endif
    ucs_status_t status;
    if(request->is_cont){
      status =
          ucp_get_nbi(request->ep, (void *)request->buf, request->size,
                      request->remote_data_addr, request->remote_data_rkey);
    } else {
      switch (request->nc_strategy)
      {
      case NC_PACKING:
      // PACKING
        status =
          ucp_get_nbi(request->ep, (void *)request->packed_buf, request->size,
                      request->remote_data_addr, request->remote_data_rkey);
        break;
      case NC_DIRECT_SEND:
        // DIRECT_SEND
        for(int k = 0; k < request->count; ++k){
          for(int i = 0; i < request->num_cont_blocks; ++i) {
            status = ucp_get_nbi(request->ep, request->buf + request->dtype_displacements[i] + k * request->dtype_extent, 
              request->dtype_lengths[i], request->remote_data_addr + request->dtype_displacements[i] + k * request->dtype_extent,
              request->remote_data_rkey);

            assert(status == UCS_OK || status == UCS_INPROGRESS);
          }
        }
        break;
      case NC_OPT_PACKING:
        status =
          ucp_get_nbi(request->ep, (void *)request->packed_buf, request->size,
                      request->remote_data_addr, request->remote_data_rkey);
        break;
      default:
        break;
      }
      
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
  if (__builtin_expect(request->flag >= request->operation_number * 2 + 2 &&
                           request->ucx_request_flag_transfer == NULL &&
                           request->ucx_request_data_transfer == NULL,
                       1)) {
    // request is finished

    if(!(request->is_cont) && request->nc_strategy == NC_PACKING){
      int position = 0;

      MPI_Unpack(request->packed_buf, request->pack_size, &position,
        request->buf, request->count, request->dtype, 
        request->communicators->original_communicator);
    }

    if(!(request->is_cont)) {
      int position = 0;
      switch(request->nc_strategy) {
      case NC_PACKING:

        MPI_Unpack(request->packed_buf, request->pack_size, &position,
          request->buf, request->count, request->dtype, 
          request->communicators->original_communicator);
        break;
      case NC_OPT_PACKING:
        opt_unpack(request);
        break;
      default:
        break;
      }
    }


    *flag = 1;
#ifdef DISTINGUISH_ACTIVE_REQUESTS
    request->active = 0;
#endif
    set_mpi_status(request, status);
  } else {
    *flag = 0;
  }
  return MPI_SUCCESS;
}

LINKAGE_TYPE void progress_request(MPIOPT_Request *request) {
  int flag;
  // progress the fallback communication
  request->test_fn(request, &flag, MPI_STATUSES_IGNORE);
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

LINKAGE_TYPE int test_fallback(MPIOPT_Request *request, int *flag,
                               MPI_Status *status) {
  return MPI_Test(&request->backup_request, flag, status);
}

// does nothing (for non-active requests)
LINKAGE_TYPE int test_empty(MPIOPT_Request *request, int *flag,
                            MPI_Status *status) {
  *flag = 1;
  return MPI_SUCCESS;
}
