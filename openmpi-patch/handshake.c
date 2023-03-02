#include "request_type.h"

#include "globals.h"
#include "handshake.h"
#include "settings.h"
#include "test.h"

#include "debug.h"
#include "mpi-internals.h"

#include <stdlib.h>
#include <unistd.h>

LINKAGE_TYPE void complete_handshake(MPIOPT_Request *request);

LINKAGE_TYPE int progress_send_request_handshake_begin(MPIOPT_Request *request,
                                                       int *flag,
                                                       MPI_Status *status) {
#ifndef NDEBUG
  add_operation_to_trace(request, "Progress_Request");
#endif
  assert(request->type == SEND_REQUEST_TYPE_HANDSHAKE_INITIATED);
  MPI_Comm comm_to_use =
      request->communicators->handshake_response_communicator;
  assert(request->remote_data_addr == NULL);
  int local_flag = 0;
  MPI_Test(&request->backup_request, &local_flag, status); // payload

  if (local_flag) {
    // check if handshake response arrived
    local_flag = 0;
    MPI_Iprobe(request->dest, request->tag, comm_to_use, &local_flag,
               MPI_STATUS_IGNORE);
    if (local_flag) {
      // found matching counterpart
      complete_handshake(request);
      set_request_type(request, SEND_REQUEST_TYPE);
      request->flag = 4;
    } else {
      // indicate that this request has finished
      request->flag = 4;
      // the Ssend was successful, meaning the other process has NOT matched
      // with a persistent operation
      set_request_type(request, SEND_REQUEST_TYPE_USE_FALLBACK);
#ifndef NDEBUG
      add_operation_to_trace(
          request, "Handshake failed: no response in time, use fallback");
#endif
    }
    // local op has finished regardless if handshake was successful or not
    *flag = 1;

  } // end if payload was received
}

LINKAGE_TYPE int progress_recv_request_handshake_begin(MPIOPT_Request *request,
                                                       int *flag,
                                                       MPI_Status *status) {
#ifndef NDEBUG
  add_operation_to_trace(request, "Progress_Request");
#endif
  assert(request->type == RECV_REQUEST_TYPE_HANDSHAKE_INITIATED);

  int local_flag = 0;
  // check if the payload has arrived
  MPI_Iprobe(request->dest, request->tag,
             request->communicators->original_communicator, &local_flag,
             MPI_STATUS_IGNORE);

  if (local_flag) {
    local_flag = 0;
    MPI_Comm comm_to_use = request->communicators->handshake_communicator;

    // check for handshake
    MPI_Iprobe(request->dest, request->tag, comm_to_use, &local_flag,
               MPI_STATUS_IGNORE);
    if (local_flag) {
      // found matching handshake
      complete_handshake(request);
      set_request_type(request, RECV_REQUEST_TYPE_HANDSHAKE_IN_PROGRESS);
    } else {
      set_request_type(request, RECV_REQUEST_TYPE_USE_FALLBACK);
    }
    // post the matching receive
    assert(request->backup_request == MPI_REQUEST_NULL);
    // blocking, as we have probed before
    MPI_Recv(request->buf, request->size, MPI_BYTE, request->dest, request->tag,
             request->communicators->original_communicator, status);
    *flag = 1;
  } // end if payload arrived
}

// exchanges the RDMA info and maps all mem for RDMA op
LINKAGE_TYPE void send_rdma_info(MPIOPT_Request *request) {

  assert(request->type == SEND_REQUEST_TYPE_HANDSHAKE_NOT_STARTED ||
         request->type == RECV_REQUEST_TYPE_HANDSHAKE_NOT_STARTED);
#ifndef NDEBUG
  add_operation_to_trace(request, "Initialize handshake");
#endif

  uint64_t flag_ptr = &request->flag;
  uint64_t data_ptr = request->buf;
  // MPIOPT_Request info_to_send;

  ompi_osc_ucx_module_t *module =
      (ompi_osc_ucx_module_t *)global_comm_win->w_osc_module;
  ucp_ep_h ep = request->ep;

  ucp_context_h context = mca_osc_ucx_component.ucp_context;
  // prepare buffer for RDMA access:
  ucp_mem_map_params_t mem_params;
  // ucp_mem_attr_t mem_attrs;
  ucs_status_t ucp_status;
  // init mem params
  memset(&mem_params, 0, sizeof(ucp_mem_map_params_t));

  mem_params.address = request->buf;
  mem_params.length = request->size;
  // we need to tell ucx what fields are valid
  mem_params.field_mask =
      UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH;

  ucp_status = ucp_mem_map(context, &mem_params, &request->mem_handle_data);
  assert(ucp_status == UCS_OK && "Error in register mem for RDMA operation");

  void *rkey_buffer_data;
  size_t rkey_size_data;

  // pack a remote memory key
  ucp_status = ucp_rkey_pack(context, request->mem_handle_data,
                             &rkey_buffer_data, &rkey_size_data);
  assert(ucp_status == UCS_OK && "Error in register mem for RDMA operation");

  memset(&mem_params, 0, sizeof(ucp_mem_map_params_t));

  mem_params.address = flag_ptr;
  mem_params.length = sizeof(int);
  // we need to tell ucx what fields are valid
  mem_params.field_mask =
      UCP_MEM_MAP_PARAM_FIELD_ADDRESS | UCP_MEM_MAP_PARAM_FIELD_LENGTH;

  void *rkey_buffer_flag;
  size_t rkey_size_flag;

  ucp_status = ucp_mem_map(context, &mem_params, &request->mem_handle_flag);
  assert(ucp_status == UCS_OK && "Error in register mem for RDMA operation");

  // pack a remote memory key
  ucp_status = ucp_rkey_pack(context, request->mem_handle_flag,
                             &rkey_buffer_flag, &rkey_size_flag);
  assert(ucp_status == UCS_OK && "Error in register mem for RDMA operation");

  size_t msg_size = sizeof(size_t) * 2 + sizeof(uint64_t) * 2 + rkey_size_data +
                    rkey_size_flag + sizeof(uint64_t) * 2;
  request->rdma_info_buf = calloc(msg_size, 1);

  // populate the buffer
  char *current_pos = request->rdma_info_buf;
  *(size_t *)current_pos = rkey_size_data;
  current_pos += sizeof(size_t);
  *(size_t *)current_pos = rkey_size_flag;
  current_pos += sizeof(size_t);
  *(u_int64_t *)current_pos = data_ptr;
  current_pos += sizeof(u_int64_t);
  *(u_int64_t *)current_pos = flag_ptr;
  current_pos += sizeof(u_int64_t);
  memcpy(current_pos, rkey_buffer_data, rkey_size_data);
  current_pos += rkey_size_data;
  current_pos += sizeof(u_int64_t); // null termination
  memcpy(current_pos, rkey_buffer_flag, rkey_size_flag);
  current_pos += rkey_size_flag;
  current_pos += sizeof(u_int64_t); // null termination

  assert(msg_size + request->rdma_info_buf == current_pos);

  MPI_Comm comm_to_use = request->communicators->handshake_communicator;
  if (is_recv_type(request)) {
    comm_to_use = request->communicators->handshake_response_communicator;
  }

  MPI_Issend(request->rdma_info_buf, msg_size, MPI_BYTE, request->dest,
             request->tag, comm_to_use, &request->rdma_exchange_request_send);

  // free temp buf
  ucp_rkey_buffer_release(rkey_buffer_flag);
  ucp_rkey_buffer_release(rkey_buffer_data);

  if (is_recv_type(request)) {
    set_request_type(request, RECV_REQUEST_TYPE_HANDSHAKE_INITIATED);
  } else {
    set_request_type(request, SEND_REQUEST_TYPE_HANDSHAKE_INITIATED);
  }
}

LINKAGE_TYPE void complete_handshake(MPIOPT_Request *request) {

#ifndef NDEBUG
  add_operation_to_trace(request, "Handshake response");
#endif

  MPI_Comm comm_to_use = request->communicators->handshake_communicator;
  if (is_sending_type(request)) {
    comm_to_use = request->communicators->handshake_response_communicator;
  }

  MPI_Status status;
  // here, we can use blocking probe: we have i-probed before
  MPI_Probe(request->dest, request->tag, comm_to_use, &status);

  int count = 0;
  MPI_Get_count(&status, MPI_BYTE, &count);

  char *tmp_buf = calloc(count, 1);

  // receive the handshake data
  MPI_Recv(tmp_buf, count, MPI_BYTE, request->dest, request->tag, comm_to_use,
           MPI_STATUS_IGNORE);

  size_t rkey_size_flag;
  size_t rkey_size_data;
  // read the buffer
  char *current_pos = tmp_buf;
  rkey_size_data = *(size_t *)current_pos;
  current_pos += sizeof(size_t);
  rkey_size_flag = *(size_t *)current_pos;
  current_pos += sizeof(size_t);
  request->remote_data_addr = *(u_int64_t *)current_pos;
  current_pos += sizeof(u_int64_t);
  request->remote_flag_addr = *(u_int64_t *)current_pos;
  current_pos += sizeof(u_int64_t);
  ucp_ep_rkey_unpack(request->ep, current_pos, &request->remote_data_rkey);
  current_pos += rkey_size_data;
  current_pos += sizeof(u_int64_t); // null termination
  ucp_ep_rkey_unpack(request->ep, current_pos, &request->remote_flag_rkey);
  current_pos += rkey_size_flag;
  current_pos += sizeof(u_int64_t); // null termination

  assert(count + tmp_buf == current_pos);

  free(tmp_buf);

  // TODO this may deadlock (?)
  MPI_Wait(&request->rdma_exchange_request_send, MPI_STATUS_IGNORE);
  // the other process has to recv the matching handshake msg sometime

#ifndef NDEBUG
  add_operation_to_trace(request, "completed Handshake");
#endif
}