#ifndef MPIOPT_REQUEST_TYPE_H
#define MPIOPT_REQUEST_TYPE_H

#define RECV_REQUEST_TYPE 1
#define SEND_REQUEST_TYPE 2
#define SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION 3
#define RECV_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION 4
#define SEND_REQUEST_TYPE_USE_FALLBACK 5
#define RECV_REQUEST_TYPE_USE_FALLBACK 6

// this request is stuck, so it should not be progressed
#define RECV_REQUEST_TYPE_NULL 7
#define SEND_REQUEST_TYPE_NULL 8

#include <mpi.h>

#include "settings.h"

#include <stdbool.h>
#include <ucp/api/ucp.h>
#include <unistd.h>

#include "mpi-internals.h"

struct mpiopt_request {
  // this way it it can be used as a normal request ptr as well
  struct ompi_request_t original_request;
  int flag;
  int flag_buffer;
  uint64_t remote_data_addr;
  uint64_t remote_flag_addr;
  ucp_rkey_h remote_data_rkey;
  ucp_rkey_h remote_flag_rkey;
  void *buf;
  size_t size;

  //datatype metadata
  char is_cont;
  void *packed_buf;
  size_t pack_size;
  size_t dtype_size;
  size_t dtype_extent;
  int count;
  MPI_Datatype dtype;
  
  // initialized locally
  void *ucx_request_data_transfer;
  void *ucx_request_flag_transfer;
  int operation_number;
  int type;
  ucp_mem_h mem_handle_data;
  ucp_mem_h mem_handle_flag;
  ucp_ep_h
      ep; // save used endpoint, so we dont have to look it up over and over#
  struct communicator_info *communicators;
  // necessary for backup in case no other persistent op matches:
  MPI_Request backup_request;
  int tag;
  int dest;
  // MPI_Request rdma_exchange_request;
  MPI_Request rdma_exchange_request_send;
  void *rdma_info_buf;
  // struct mpiopt_request* rdma_exchange_buffer;
#ifdef BUFFER_CONTENT_CHECKING
  void *checking_buf;
  MPI_Request chekcking_request;
#endif
#ifdef DISTINGUISH_ACTIVE_REQUESTS
  int active;
#endif
};
typedef struct mpiopt_request MPIOPT_Request;

static inline is_sending_type(MPIOPT_Request *request) {
  return request->type == SEND_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION ||
         request->type == SEND_REQUEST_TYPE ||
         request->type == SEND_REQUEST_TYPE_USE_FALLBACK ||
         request->type == SEND_REQUEST_TYPE_NULL;
}
static inline is_recv_type(MPIOPT_Request *request) {
  return request->type == RECV_REQUEST_TYPE_SEARCH_FOR_RDMA_CONNECTION ||
         request->type == RECV_REQUEST_TYPE ||
         request->type == RECV_REQUEST_TYPE_USE_FALLBACK ||
         request->type == RECV_REQUEST_TYPE_NULL;
}

#endif // MPIOPT_REQUEST_TYPE_H
