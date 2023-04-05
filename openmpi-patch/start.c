#include "globals.h"
#include "settings.h"

#include "handshake.h"
#include "start.h"

#include <assert.h>
#include <ucp/api/ucp.h>

#include "debug.h"
#include "mpi-internals.h"
#include "pack.h"

#include <stdlib.h>
#include <unistd.h>

static void empty_function_in_start_c(void *request, ucs_status_t status) {
  // callback if flush is completed
}

LINKAGE_TYPE int b_send(MPIOPT_Request *request) {

  if(!(request->is_cont)) {
    int position = 0;
    switch(request->nc_strategy) {
    case NC_PACKING:

      MPI_Pack(request->buf, request->count, 
        request->dtype, request->packed_buf, request->pack_size, 
        &position, request->communicators->original_communicator);
      break;
    case NC_OPT_PACKING:
      opt_pack(request);
      break;
    
    case NC_MIXED:
      opt_pack_threshold(request);
      break;
    default:
      break;
    }
  }

#ifndef NDEBUG
  add_operation_to_trace(request, "MPI_Start");
#endif
  request->operation_number++;
  assert(request->flag >= request->operation_number * 2 &&
         request->type == SEND_REQUEST_TYPE);
  assert(request->ucx_request_data_transfer == NULL &&
         request->ucx_request_flag_transfer == NULL);
#ifdef DISTINGUISH_ACTIVE_REQUESTS
  assert(request->active == 0);
  request->active = 1;
#endif

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

      switch (request->nc_strategy)
      {
      case NC_PACKING:
        // PACKING
        status =
          ucp_put_nbi(request->ep, request->packed_buf, request->pack_size,
                      request->remote_data_addr, request->remote_data_rkey);
        break;
      case NC_DIRECT_SEND:
        // DIRECT_SEND

        for(int k = 0; k < request->count; ++k){
          for(int i = 0; i < request->num_cont_blocks; ++i) {
            status = ucp_put_nbi(request->ep, request->buf + request->dtype_displacements[i] + k * request->dtype_extent, 
              request->dtype_lengths[i], request->remote_data_addr + request->dtype_displacements[i] + k * request->dtype_extent,
              request->remote_data_rkey);
            assert(status == UCS_OK || status == UCS_INPROGRESS);
          }
        }
        break;
      case NC_OPT_PACKING:
        status =
          ucp_put_nbi(request->ep, request->packed_buf, request->pack_size,
                      request->remote_data_addr, request->remote_data_rkey);
        break;
      
      case NC_MIXED:
        for(int k = 0; k < request->count; ++k){
          for(int i = 0; i < request->num_cont_blocks; ++i) {
            if(request->dtype_lengths[i] > request->threshold) {
              status = ucp_put_nbi(request->ep, request->buf + request->dtype_displacements[i] + k * request->dtype_extent, 
                request->dtype_lengths[i], request->remote_data_addr + request->dtype_displacements[i] + k * request->dtype_extent,
                request->remote_data_rkey);
              assert(status == UCS_OK || status == UCS_INPROGRESS);
            }
          }
        }
        status =
          ucp_put_nbi(request->ep, request->packed_buf, request->pack_size,
                      request->remote_packed_addr, request->remote_packed_data_rkey);

        break;

      default:
        break;
      }
      
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
  return MPI_SUCCESS;
}

LINKAGE_TYPE int b_recv(MPIOPT_Request *request) {
#ifndef NDEBUG
  add_operation_to_trace(request, "MPI_Start");
#endif
  request->operation_number++;
  assert(request->flag >= request->operation_number * 2 &&
         request->type == RECV_REQUEST_TYPE);
  assert(request->ucx_request_data_transfer == NULL &&
         request->ucx_request_flag_transfer == NULL);
#ifdef DISTINGUISH_ACTIVE_REQUESTS
  assert(request->active == 0);
  request->active = 1;
#endif
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

      switch (request->nc_strategy)
      {
      case NC_PACKING:
        // PACKING
        status =
          ucp_get_nbi(request->ep, (void *)request->packed_buf, request->pack_size,
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
          ucp_get_nbi(request->ep, (void *)request->packed_buf, request->pack_size,
                      request->remote_data_addr, request->remote_data_rkey);
        break;

      case NC_MIXED:
        for(int k = 0; k < request->count; ++k){
          for(int i = 0; i < request->num_cont_blocks; ++i) {
            if(request->dtype_lengths[i] > request->threshold) {
              status = ucp_get_nbi(request->ep, request->buf + request->dtype_displacements[i] + k * request->dtype_extent, 
                request->dtype_lengths[i], request->remote_data_addr + request->dtype_displacements[i] + k * request->dtype_extent,
                request->remote_data_rkey);
              assert(status == UCS_OK || status == UCS_INPROGRESS);
            }
          }
        }
        status =
          ucp_get_nbi(request->ep, (void *)request->packed_buf, request->pack_size,
                      request->remote_packed_addr, request->remote_packed_data_rkey);

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
  return MPI_SUCCESS;
}

LINKAGE_TYPE int
start_send_when_searching_for_connection(MPIOPT_Request *request) {
  ++request->operation_number;
#ifndef NDEBUG
  add_operation_to_trace(request, "MPI_Start");
#endif
  assert(request->operation_number == 1);
  assert(request->type == SEND_REQUEST_TYPE_HANDSHAKE_NOT_STARTED);
#ifdef DISTINGUISH_ACTIVE_REQUESTS
  assert(request->active == 0);
  request->active = 1;
#endif
  set_request_type(request, SEND_REQUEST_TYPE_HANDSHAKE_INITIATED);
  send_rdma_info(request); // begin handshake, changes request type
  // always post a normal msg, in case of fallback to normal comm is needed
  // for the first time, the receiver will post a matching recv
  assert(request->backup_request == MPI_REQUEST_NULL);
  return MPI_Issend(request->buf, request->count, request->dtype, request->dest,
                    request->tag, request->communicators->original_communicator,
                    &request->backup_request);
}

LINKAGE_TYPE int
start_recv_when_searching_for_connection(MPIOPT_Request *request) {
  ++request->operation_number;
#ifndef NDEBUG
  add_operation_to_trace(request, "MPI_Start");
#endif
  assert(request->operation_number == 1);
  assert(request->type == RECV_REQUEST_TYPE_HANDSHAKE_NOT_STARTED);
#ifdef DISTINGUISH_ACTIVE_REQUESTS
  assert(request->active == 0);
  request->active = 1;
#endif
  set_request_type(request, RECV_REQUEST_TYPE_HANDSHAKE_INITIATED);
  // we will just wait for the handshake and the payload
  return MPI_SUCCESS;
}

LINKAGE_TYPE int start_send_fallback(MPIOPT_Request *request) {
#ifndef NDEBUG
  add_operation_to_trace(request, "MPI_Start");
#endif
#ifdef DISTINGUISH_ACTIVE_REQUESTS
  assert(request->active == 0);
  request->active = 1;
#endif

  request->operation_number++;
  assert(request->type == SEND_REQUEST_TYPE_USE_FALLBACK);
  assert(request->backup_request == MPI_REQUEST_NULL);
  return MPI_Isend(request->buf, request->count, request->dtype, request->dest,
                   request->tag, request->communicators->original_communicator,
                   &request->backup_request);
}

LINKAGE_TYPE int start_recv_fallback(MPIOPT_Request *request) {
#ifndef NDEBUG
  add_operation_to_trace(request, "MPI_Start");
#endif
#ifdef DISTINGUISH_ACTIVE_REQUESTS
  assert(request->active == 0);
  request->active = 1;
#endif

  request->operation_number++;
  assert(request->type == RECV_REQUEST_TYPE_USE_FALLBACK);
  assert(request->backup_request == MPI_REQUEST_NULL);
  return MPI_Irecv(request->buf, request->count, request->dtype, request->dest,
                   request->tag, request->communicators->original_communicator,
                   &request->backup_request);
}
