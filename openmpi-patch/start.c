#include "globals.h"
#include "settings.h"

#include "handshake.h"
#include "start.h"

#include <assert.h>
#include <ucp/api/ucp.h>

#include "debug.h"
#include "mpi-internals.h"

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
#ifndef NDEBUG
    add_operation_to_trace(request, "send pushes data");
#endif
    request->flag_buffer = request->operation_number * 2 + 2;


    ucs_status_t status;
    if(request->is_cont){
      status =
          ucp_put_nbi(request->ep, request->buf, request->size,
                      request->remote_data_addr, request->remote_data_rkey);
    } else {
      status =
          ucp_put_nbi(request->ep, request->packed_buf, request->size,
                      request->remote_data_addr, request->remote_data_rkey);
    }

    
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
#ifndef NDEBUG
    add_operation_to_trace(request, "recv fetches data");
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

  send_rdma_info(request); // begin handshake
  // always post a normal msg, in case of fallback to normal comm is needed
  // for the first time, the receiver will post a matching recv
  assert(request->backup_request == MPI_REQUEST_NULL);
  MPI_Issend(request->buf, request->count, request->dtype, request->dest, request->tag,
             request->communicators->original_communicator,
             &request->backup_request);
  // and listen for rdma handshake
  progress_send_request_waiting_for_rdma(request);
}

LINKAGE_TYPE void
start_recv_when_searching_for_connection(MPIOPT_Request *request) {

  assert(request->operation_number == 1);

  send_rdma_info(request); // begin handshake

  progress_recv_request_waiting_for_rdma(request);
  // the recv will be posted, after a check for the handshake was done
}

LINKAGE_TYPE int MPIOPT_Start_send_internal(MPIOPT_Request *request) {

#ifdef DISTINGUISH_ACTIVE_REQUESTS
  assert(request->active == 0);
  request->active = 1;
#endif

  assert(is_sending_type(request));

  // TODO atomic increment for multi threading
  request->operation_number++;

  assert(request->flag >= request->operation_number * 2 || request->type==SEND_REQUEST_TYPE_USE_FALLBACK);
  assert(request->ucx_request_data_transfer == NULL &&
         request->ucx_request_flag_transfer == NULL);

  if (__builtin_expect(request->type == SEND_REQUEST_TYPE, 1)) {

    // Pack data into cont. buffer
    if(!(request->is_cont)){
      printf("packing...\n");
      int position = 0;

      MPI_Pack(request->buf, request->count, 
        request->dtype, request->packed_buf, request->pack_size, &position, request->communicators->original_communicator);
    }
    b_send(request);

  } else if (request->type == SEND_REQUEST_TYPE_HANDSHAKE_NOT_STARTED) {

    start_send_when_searching_for_connection(request);
  } else if (request->type == SEND_REQUEST_TYPE_USE_FALLBACK) {
    assert(request->backup_request == MPI_REQUEST_NULL);
    MPI_Isend(request->buf, request->count, request->dtype, request->dest,
              request->tag, request->communicators->original_communicator,
              &request->backup_request);

  } else {

    assert(request->type != SEND_REQUEST_TYPE_HANDSHAKE_INITIATED);
    assert(request->type != SEND_REQUEST_TYPE_HANDSHAKE_IN_PROGRESS);
    assert(false && "Error: uninitialized Request");
  }
#ifdef BUFFER_CONTENT_CHECKING
  assert(request->chekcking_request == MPI_REQUEST_NULL);
  MPI_Isend(request->buf, request->count, request->dtype, request->dest, request->tag,
            request->communicators->checking_communicator,
            &request->chekcking_request);

#endif
}

LINKAGE_TYPE int MPIOPT_Start_recv_internal(MPIOPT_Request *request) {

#ifdef DISTINGUISH_ACTIVE_REQUESTS
  assert(request->active == 0);
  request->active = 1;
#endif

  assert(is_recv_type(request));

  // TODO atomic increment for multi threading
  request->operation_number++;
  assert(request->flag >= request->operation_number * 2 || request->type==RECV_REQUEST_TYPE_USE_FALLBACK);
  assert(request->ucx_request_data_transfer == NULL &&
         request->ucx_request_flag_transfer == NULL);

  if (__builtin_expect(request->type == RECV_REQUEST_TYPE, 1)) {
    b_recv(request);

  } else if (request->type == RECV_REQUEST_TYPE_HANDSHAKE_NOT_STARTED) {
    start_recv_when_searching_for_connection(request);

  } else if (request->type == RECV_REQUEST_TYPE_USE_FALLBACK) {
    assert(request->backup_request == MPI_REQUEST_NULL);
    MPI_Irecv(request->buf, request->count, request->dtype, request->dest,
              request->tag, request->communicators->original_communicator,
              &request->backup_request);
  } else {
    assert(request->type != RECV_REQUEST_TYPE_HANDSHAKE_INITIATED);
    assert(request->type != RECV_REQUEST_TYPE_HANDSHAKE_IN_PROGRESS);
    assert(false && "Error: uninitialized Request");
  }

#ifdef BUFFER_CONTENT_CHECKING
  assert(request->chekcking_request == MPI_REQUEST_NULL);
  MPI_Irecv(request->checking_buf, request->count, request->dtype, request->dest,
            request->tag, request->communicators->checking_communicator,
            &request->chekcking_request);

#endif
}

LINKAGE_TYPE int MPIOPT_Start_internal(MPIOPT_Request *request) {
#ifndef NDEBUG
  add_operation_to_trace(request, "MPI_Start");
#endif
  if (is_sending_type(request)) {
    return MPIOPT_Start_send_internal(request);
  } else {
    return MPIOPT_Start_recv_internal(request);
  }
}