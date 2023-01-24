#include "globals.h"
#include "request_type.h"
#include "settings.h"

#include <assert.h>
#include <stdlib.h>

// globals

MPI_Win global_comm_win =MPI_WIN_NULL;

struct communicator_info *communicator_array = NULL;
int communicator_array_size = 0;

#ifdef SUMMARY_STATISTIC_PRINTING
unsigned int crosstalk_counter = 0;
#endif

struct list_elem *request_list_head = NULL;

// requests to free (defer free of some ressources until finalize, so that
// request free is local operation)
struct list_elem *to_free_list_head = NULL;
// tell other rank, that it should post a matching receive for all unsuccessful
// handshakes
struct list_elem_int *msg_send = NULL;

// add it at beginning of list
LINKAGE_TYPE void add_request_to_list(MPIOPT_Request *request) {
  struct list_elem *new_elem = malloc(sizeof(struct list_elem));
  new_elem->elem = request;
  new_elem->next = request_list_head->next;
  request_list_head->next = new_elem;
}

LINKAGE_TYPE void remove_request_from_list(MPIOPT_Request *request) {
  struct list_elem *previous_elem = request_list_head;
  struct list_elem *current_elem = request_list_head->next;
  assert(current_elem != NULL);
  while (current_elem->elem != request) {
    previous_elem = current_elem;
    current_elem = previous_elem->next;
    assert(current_elem != NULL);
  }
  // remove elem from list
  previous_elem->next = current_elem->next;
  free(current_elem);
}