#include "globals.h"
#include "settings.h"

#include "handshake.h"
#include "start.h"

#include <assert.h>
#include <ucp/api/ucp.h>

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

static void empty_function_in_start_c(void *request, ucs_status_t status) {
  // callback if flush is completed
}

LINKAGE_TYPE void b_send(MPIOPT_Request *request) {

  if (__builtin_expect(request->flag == request->operation_number * 2 + 1, 1)) {
    // increment: signal that WE finish the operation on the remote
    request->flag++;
    // no possibility of data-race, the remote will wait for us to put the data
    assert(request->flag == request->operation_number * 2 + 2);
    // start rdma data transfer
#ifdef STATISTIC_PRINTING
    printf("send pushes data\n");
#endif
    request->flag_buffer = request->operation_number * 2 + 2;
    ucs_status_t status =
        ucp_put_nbi(request->ep, request->buf, request->size,
                    request->remote_data_addr, request->remote_data_rkey);
    // ensure order:
    status = ucp_worker_fence(mca_osc_ucx_component.ucp_worker);
    status = ucp_put_nbi(request->ep, &request->flag_buffer, sizeof(int),
                         request->remote_flag_addr, request->remote_flag_rkey);
    assert(request->ucx_request_data_transfer == NULL);
    request->ucx_request_data_transfer =
        ucp_ep_flush_nb(request->ep, 0, empty_function_in_start_c);

    // TODO do I call progress here?
    ucp_worker_progress(mca_osc_ucx_component.ucp_worker);

  } else {
    assert(request->flag == request->operation_number * 2);
    request->flag_buffer = request->operation_number * 2 + 1;
    // give him the flag that we are ready: he will RDMA get the data
    ucs_status_t status =
        ucp_put_nbi(request->ep, &request->flag_buffer, sizeof(int),
                    request->remote_flag_addr, request->remote_flag_rkey);
    assert(request->ucx_request_flag_transfer == NULL);
    request->ucx_request_flag_transfer =
        ucp_ep_flush_nb(request->ep, 0, empty_function_in_start_c);
    // TODO do I call progress here?
    ucp_worker_progress(mca_osc_ucx_component.ucp_worker);
  }
}

LINKAGE_TYPE void b_recv(MPIOPT_Request *request) {
  if (__builtin_expect(request->flag == request->operation_number * 2 + 1, 0)) {

    request->flag++; // recv is done at our side
    // no possibility of data race, WE will advance the comm
    assert(request->flag == request->operation_number * 2 + 2);
    // start rdma data transfer
#ifdef STATISTIC_PRINTING
    printf("recv fetches data\n");
#endif
    ucs_status_t status =
        ucp_get_nbi(request->ep, (void *)request->buf, request->size,
                    request->remote_data_addr, request->remote_data_rkey);

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
    assert(request->ucx_request_data_transfer == NULL);
    request->ucx_request_data_transfer =
        ucp_ep_flush_nb(request->ep, 0, empty_function_in_start_c);

    // TODO do I call progress here?
    ucp_worker_progress(mca_osc_ucx_component.ucp_worker);

  } else {
    assert(request->flag == request->operation_number * 2);
    // request->flag = READY_TO_RECEIVE;
    request->flag_buffer = request->operation_number * 2 + 1;
    // give him the flag that we are ready: he will RDMA put the data
    ucs_status_t status =
        ucp_put_nbi(request->ep, &request->flag_buffer, sizeof(int),
                    request->remote_flag_addr, request->remote_flag_rkey);
    assert(status == UCS_OK || status == UCS_INPROGRESS);
    assert(request->ucx_request_flag_transfer == NULL);
    request->ucx_request_flag_transfer =
        ucp_ep_flush_nb(request->ep, 0, empty_function_in_start_c);
    // TODO do I call progress here?
    ucp_worker_progress(mca_osc_ucx_component.ucp_worker);
  }
}

LINKAGE_TYPE void
start_send_when_searching_for_connection(MPIOPT_Request *request) {

  assert(request->operation_number == 1);

  // always post a normal msg, in case of fallback to normal comm is needed
  // for the first time, the receiver will post a matching recv
  assert(request->backup_request == MPI_REQUEST_NULL);
  MPI_Issend(request->buf, request->size, MPI_BYTE, request->dest, request->tag,
             request->comm, &request->backup_request);
  // and listen for rdma handshake
  progress_send_request_waiting_for_rdma(request);
}

LINKAGE_TYPE void
start_recv_when_searching_for_connection(MPIOPT_Request *request) {
  assert(request->operation_number == 1);

  progress_recv_request_waiting_for_rdma(request);

  // meaning no RDMA connection is presnt
  if (request->remote_data_addr == NULL) {

    int flag;
    MPI_Iprobe(request->dest, request->tag, request->comm, &flag,
               MPI_STATUS_IGNORE);
    if (flag) {
      // if probed for matching msg failed, but this msg arrived, we can be
      // shure that no matching msg will be sent in the future
      // as msg order is defined
      assert(request->backup_request == MPI_REQUEST_NULL);
      // post the matching recv
      printf("Post RECV, Fallback in start\n");
      MPI_Irecv(request->buf, request->size, MPI_BYTE, request->dest,
                request->tag, request->comm, &request->backup_request);
    }
  } else {
    // RDMA handshake complete, we can post the matching recv
    if (request->backup_request == MPI_REQUEST_NULL) {
      MPI_Irecv(request->buf, request->size, MPI_BYTE, request->dest,
                request->tag, request->comm, &request->backup_request);
    }
  }

  // ordering guarantees, that the probe for matching msg will return true
  // before probe of the payload does
}

LINKAGE_TYPE int MPIOPT_Start_send_internal(MPIOPT_Request *request) {

  // TODO atomic increment for multi threading
  request->operation_number++;
  assert(request->flag >= request->operation_number * 2);
  assert(request->ucx_request_data_transfer == NULL &&
         request->ucx_request_flag_transfer == NULL);

  if (__builtin_expect(request->type == SEND_REQUEST_TYPE, 1)) {
    b_send(request);

  } else if (request->type == SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION) {
    start_send_when_searching_for_connection(request);
  } else if (request->type == SEND_REQUEST_TYPE_USE_FALLBACK) {
    assert(request->backup_request == MPI_REQUEST_NULL);
    MPI_Isend(request->buf, request->size, MPI_BYTE, request->dest,
              request->tag, request->comm, &request->backup_request);

  } else {
    assert(false && "Error: uninitialized Request");
  }
#ifdef BUFFER_CONTENT_CHECKING
  assert(request->chekcking_request == MPI_REQUEST_NULL);
  MPI_Isend(request->buf, request->size, MPI_BYTE, request->dest, request->tag,
            checking_communicator, &request->chekcking_request);

#endif
}

LINKAGE_TYPE int MPIOPT_Start_recv_internal(MPIOPT_Request *request) {

  // TODO atomic increment for multi threading
  request->operation_number++;
  assert(request->flag >= request->operation_number * 2);
  assert(request->ucx_request_data_transfer == NULL &&
         request->ucx_request_flag_transfer == NULL);

  if (__builtin_expect(request->type == RECV_REQUEST_TYPE, 1)) {
    b_recv(request);

  } else if (request->type == RECV_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION) {
    start_recv_when_searching_for_connection(request);

  } else if (request->type == RECV_REQUEST_TYPE_USE_FALLBACK) {
    assert(request->backup_request == MPI_REQUEST_NULL);
    MPI_Irecv(request->buf, request->size, MPI_BYTE, request->dest,
              request->tag, request->comm, &request->backup_request);
  } else {
    assert(false && "Error: uninitialized Request");
  }

#ifdef BUFFER_CONTENT_CHECKING
  assert(request->chekcking_request == MPI_REQUEST_NULL);
  MPI_Irecv(request->checking_buf, request->size, MPI_BYTE, request->dest,
            request->tag, checking_communicator, &request->chekcking_request);

#endif
}

LINKAGE_TYPE int MPIOPT_Start_internal(MPIOPT_Request *request) {

  if (request->type == SEND_REQUEST_TYPE ||
      request->type == SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION ||
      request->type == SEND_REQUEST_TYPE_USE_FALLBACK) {
    return MPIOPT_Start_send_internal(request);
  } else {
    return MPIOPT_Start_recv_internal(request);
  }
}