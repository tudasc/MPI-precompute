#include "handshake.h"
#include "settings.h"
#include "test.h"

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

LINKAGE_TYPE void
progress_send_request_waiting_for_rdma(MPIOPT_Request *request) {

  MPI_Comm comm_to_use = handshake_response_communicator;
  assert(request->type == SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION);
  if (request->remote_data_addr == NULL) {

    int flag;

    MPI_Iprobe(request->dest, request->tag, comm_to_use, &flag,
               MPI_STATUS_IGNORE);
    if (flag) {
      // found matching counterpart
      receive_rdma_info(request);
    }
  }

  if (request->backup_request != MPI_REQUEST_NULL &&
      request->operation_number == 1) {
    int flag = 0;
    MPI_Test(&request->backup_request, &flag, MPI_STATUS_IGNORE); // payload
    // try one last time to get the handshake
    if (flag) {
      if (request->remote_data_addr == NULL) {
        MPI_Iprobe(request->dest, request->tag, comm_to_use, &flag,
                   MPI_STATUS_IGNORE);
        if (flag) {
          // found matching counterpart
          receive_rdma_info(request);
        }
      }

      // indicate that this request has finished
      request->flag = 4;

      if (request->remote_data_addr == NULL) {
        // the Ssend was successful, meaning the other process has NOT matched
        // with a persistent operation
        request->type = SEND_REQUEST_TYPE_USE_FALLBACK;
#ifdef STATISTIC_PRINTING
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        printf("Rank %d: SEND: No RDMA connection, use normal MPI\n", rank);
#endif
      } else {
        request->type = SEND_REQUEST_TYPE;
      }
    }
  }
}

LINKAGE_TYPE void
progress_recv_request_waiting_for_rdma(MPIOPT_Request *request) {

  int flag = 0;
  // check if the payload has arrived
  MPI_Iprobe(request->dest, request->tag, request->comm, &flag,
             MPI_STATUS_IGNORE);

  if (flag && request->remote_data_addr == NULL) {

    int flag = 0;
    MPI_Comm comm_to_use = handshake_communicator;
    assert(request->type == RECV_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION);

    MPI_Iprobe(request->dest, request->tag, comm_to_use, &flag,
               MPI_STATUS_IGNORE);
    if (flag) {
      // found matching counterpart
      receive_rdma_info(request);
    }
    // post the matching receive
    assert(request->backup_request == MPI_REQUEST_NULL);
    MPI_Recv(request->buf, request->size, MPI_BYTE, request->dest, request->tag,
             request->comm, &request->backup_request);
    // we have probed, it can be received

    // at this point the handshake was successful, or will never arrive

    request->flag = 4; // done with this communication
    if (request->remote_data_addr == NULL) {
      request->type = RECV_REQUEST_TYPE_USE_FALLBACK;
    } else {
      request->type = RECV_REQUEST_TYPE;
    }
  }
}

// exchanges the RDMA info and maps all mem for RDMA op
LINKAGE_TYPE void send_rdma_info(MPIOPT_Request *request) {

  assert(request->type == SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION ||
         request->type == RECV_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION);

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

  MPI_Comm comm_to_use = handshake_communicator;
  if (request->type == RECV_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION) {
    comm_to_use = handshake_response_communicator;
  }

  MPI_Issend(request->rdma_info_buf, msg_size, MPI_BYTE, request->dest,
             request->tag, comm_to_use, &request->rdma_exchange_request_send);

  // free temp buf
  ucp_rkey_buffer_release(rkey_buffer_flag);
  ucp_rkey_buffer_release(rkey_buffer_data);
}

LINKAGE_TYPE void receive_rdma_info(MPIOPT_Request *request) {

  assert(request->type == SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION ||
         request->type == RECV_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION);

#ifdef STATISTIC_PRINTING
  int drank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &drank);
  if (request->type == SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION) {
    printf("Rank %d: SENDING: TRY RDMA established\n", drank);
  } else {
    printf("Rank %d: RECV: TRY RDMA established \n", drank);
  }
#endif

  MPI_Comm comm_to_use = handshake_communicator;
  if (request->type == SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION) {
    comm_to_use = handshake_response_communicator;
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

  // the other process has to recv the matching handshake msg sometime
  int flag;
  MPI_Test(&request->rdma_exchange_request_send, &flag, MPI_STATUS_IGNORE);
  while (!flag) {
    progress_other_requests(request);
    MPI_Test(&request->rdma_exchange_request_send, &flag, MPI_STATUS_IGNORE);

    // TODO It MAY be the case, that the other rank frees the request before
    // establishing an RDMA connection
    // but this will not happen if a communication operations is done
  }

#ifdef STATISTIC_PRINTING

  int rank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (request->type == SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION) {
    printf("Rank %d: SENDING: RDMA connection established\n", rank);
  } else {
    printf("Rank %d: RECV: RDMA connection established \n", rank);
  }
#endif
}