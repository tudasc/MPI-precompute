#ifndef MPIOPT_REQUEST_TYPE_H
#define MPIOPT_REQUEST_TYPE_H

// TODO enum for the request type?
#define SEND_REQUEST_TYPE 1
#define RECV_REQUEST_TYPE 2

#define SEND_REQUEST_TYPE_USE_FALLBACK 3
#define RECV_REQUEST_TYPE_USE_FALLBACK 4

#define SEND_REQUEST_TYPE_HANDSHAKE_NOT_STARTED 5
#define RECV_REQUEST_TYPE_HANDSHAKE_NOT_STARTED 6

#define SEND_REQUEST_TYPE_HANDSHAKE_INITIATED 7
#define RECV_REQUEST_TYPE_HANDSHAKE_INITIATED 8

#define SEND_REQUEST_TYPE_HANDSHAKE_IN_PROGRESS 9
#define RECV_REQUEST_TYPE_HANDSHAKE_IN_PROGRESS 10

// this request is stuck, so it should not be progressed
#define SEND_REQUEST_TYPE_NULL 11
#define RECV_REQUEST_TYPE_NULL 12

struct mpiopt_request; // forward declaration of struct type
typedef struct mpiopt_request MPIOPT_Request;
// BEFORE including other headers

#include <mpi.h>

#include "settings.h"

#include "debug.h"
#include "handshake.h"
#include "start.h"
#include "test.h"

#include <stdbool.h>
#include <stdio.h>
#include <ucp/api/ucp.h>
#include <unistd.h>

#include "mpi-internals.h"
#ifndef NDEBUG
// instead of #include "debug.h" to avoid cyclic inclusion
struct debug_data;
#endif

typedef int (*mpiopt_request_start_fn_t)(MPIOPT_Request *request);
typedef int (*mpiopt_request_test_fn_t)(MPIOPT_Request *request, int *flag,
                                        MPI_Status *status);

struct mpiopt_request {
  // this way the request ptr can be used as a normal request ptr as well
  struct ompi_request_t original_request;
  mpiopt_request_start_fn_t start_fn;
  mpiopt_request_test_fn_t test_fn;
  int flag;
  int flag_buffer;
  uint64_t remote_data_addr;
  uint64_t remote_flag_addr;
  ucp_rkey_h remote_data_rkey;
  ucp_rkey_h remote_flag_rkey;
  void *buf;
  size_t size;
  // initialized locally
  void *ucx_request_data_transfer;
  void *ucx_request_flag_transfer;
  int operation_number;
  int type;
  ucp_mem_h mem_handle_data;
  ucp_mem_h mem_handle_flag;
  ucp_ep_h
      ep; // save used endpoint, so we dont have to look it up over and over
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
#ifndef NDEBUG
  struct debug_data *debug_data;
#endif
};

static inline bool is_sending_type(MPIOPT_Request *request) {
  // odd
  return request->type % 2 == 1;

  static_assert(SEND_REQUEST_TYPE_HANDSHAKE_NOT_STARTED % 2 == 1, "");
  static_assert(SEND_REQUEST_TYPE_HANDSHAKE_INITIATED % 2 == 1, "");
  static_assert(SEND_REQUEST_TYPE_HANDSHAKE_IN_PROGRESS % 2 == 1, "");
  static_assert(SEND_REQUEST_TYPE % 2 == 1, "");
  static_assert(SEND_REQUEST_TYPE_USE_FALLBACK % 2 == 1, "");
  static_assert(SEND_REQUEST_TYPE_NULL % 2 == 1, "");
}

static inline bool is_recv_type(MPIOPT_Request *request) {
  // even
  return request->type % 2 == 0;

  static_assert(RECV_REQUEST_TYPE_HANDSHAKE_NOT_STARTED % 2 == 0, "");
  static_assert(RECV_REQUEST_TYPE_HANDSHAKE_INITIATED % 2 == 0, "");
  static_assert(RECV_REQUEST_TYPE_HANDSHAKE_IN_PROGRESS % 2 == 0, "");
  static_assert(RECV_REQUEST_TYPE % 2 == 0, "");
  static_assert(RECV_REQUEST_TYPE_USE_FALLBACK % 2 == 0, "");
  static_assert(RECV_REQUEST_TYPE_NULL % 2 == 0, "");
}

// inlining should remove the switch
static inline void set_request_type(MPIOPT_Request *request, int new_type) {
  request->type = new_type;
  switch (new_type) {
  case SEND_REQUEST_TYPE:
    request->start_fn = &b_send;
    request->test_fn = &MPIOPT_Test_internal;
    break;
  case RECV_REQUEST_TYPE:
    request->start_fn = &b_recv;
    request->test_fn = &MPIOPT_Test_internal;
    break;
  case SEND_REQUEST_TYPE_USE_FALLBACK:
    request->start_fn = &start_send_fallback;
    request->test_fn = &MPIOPT_Test_internal;
    break;
  case RECV_REQUEST_TYPE_USE_FALLBACK:
    request->start_fn = &start_recv_fallback;
    request->test_fn = &MPIOPT_Test_internal;
    break;
  case SEND_REQUEST_TYPE_HANDSHAKE_NOT_STARTED:
    request->start_fn = &start_send_when_searching_for_connection;
    request->test_fn = &MPIOPT_Test_internal;
    break;
  case RECV_REQUEST_TYPE_HANDSHAKE_NOT_STARTED:
    request->start_fn = &start_recv_when_searching_for_connection;
    request->test_fn = &MPIOPT_Test_internal;
    break;
  case SEND_REQUEST_TYPE_HANDSHAKE_INITIATED:
    request->start_fn =
        NULL; // request is in progress and could not be started anyway
    request->test_fn = &MPIOPT_Test_internal;
    break;
  case RECV_REQUEST_TYPE_HANDSHAKE_INITIATED:
    request->start_fn =
        NULL; // request is in progress and could not be started anyway
    request->test_fn = &MPIOPT_Test_internal;
    break;
  case SEND_REQUEST_TYPE_HANDSHAKE_IN_PROGRESS:
    request->start_fn =
        NULL; // request is in progress and could not be started anyway
    request->test_fn = &MPIOPT_Test_internal;
    break;
  case RECV_REQUEST_TYPE_HANDSHAKE_IN_PROGRESS:
    request->start_fn =
        NULL; // request is in progress and could not be started anyway
    request->test_fn = &MPIOPT_Test_internal;
    break;
  default:
    assert(false);
  }

#ifndef NDEBUG
#define TRACE_MSG_SIZE 20
  char *msg = (char *)malloc(TRACE_MSG_SIZE);
  sprintf(msg, "Change Type to %d", new_type);
  add_operation_to_trace(request, msg);
  free(msg);
#undef TRACE_MSG_SIZE
#endif
}

#endif // MPIOPT_REQUEST_TYPE_H
