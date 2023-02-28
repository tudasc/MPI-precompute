#ifndef MPIOPT_DEBUG_H_
#define MPIOPT_DEBUG_H_

// utilities for debugging
#ifndef NDEBUG
#include "request_type.h"
#include "settings.h"

#include <assert.h>
#include <stdlib.h>

struct trace_list_elem {
  char *elem;
  struct trace_list_elem *next;
};

struct debug_data {
  struct trace_list_elem *trace_list_head;
  struct trace_list_elem *trace_list_tail;
};

void add_operation_to_trace(MPIOPT_Request *request, char *operation);

void dump_trace_to_file(MPIOPT_Request *request);

void init_debug_data(MPIOPT_Request *request);
void free_debug_data(MPIOPT_Request *request);

#endif
#endif /* MPIOPT_DEBUG_H_ */
