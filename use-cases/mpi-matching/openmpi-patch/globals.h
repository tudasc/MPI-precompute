#ifndef MPIOPT_GLOBALS_H_
#define MPIOPT_GLOBALS_H_

#include "request_type.h"
#include "settings.h"

#include <assert.h>
#include <stdlib.h>

struct list_elem_int {
  int value;
  struct list_elem_int *next;
};

// globals
// TODO refactor and have one struct for globals?
extern MPI_Win global_comm_win;

#ifdef SUMMARY_STATISTIC_PRINTING
extern unsigned int crosstalk_counter;
#endif

// linked list of all requests that we have, so that we can progress them in
// case we get stuck
struct list_elem {
  MPIOPT_Request *elem;
  struct list_elem *next;
};
extern struct list_elem *request_list_head;

// requests to free (defer free of some ressources until finalize, so that
// request free is local operation)
extern struct list_elem *to_free_list_head;
// tell other rank, that it should post a matching receive for all unsuccessful
// handshakes
extern struct list_elem_int *msg_send;

struct communicator_info {
  MPI_Comm original_communicator;
  MPI_Comm handshake_communicator;
  MPI_Comm handshake_response_communicator;
// we need a different comm here, so that send a handshake response (recv
// handshake) cannot be mistaken for another handshake-request from a send with
// the same tag
#ifdef BUFFER_CONTENT_CHECKING
  MPI_Comm checking_communicator;
#endif
};

// keep track of all the communicators
extern struct communicator_info *communicator_array;
extern int communicator_array_size;

// helper for managing the request list
// add it at beginning of list
LINKAGE_TYPE void add_request_to_list(MPIOPT_Request *request);

LINKAGE_TYPE void remove_request_from_list(MPIOPT_Request *request);

#endif /* MPIOPT_GLOBALS_H_ */
