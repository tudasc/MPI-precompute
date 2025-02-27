// utilities for debugging
#ifndef NDEBUG

#define _POSIX_C_SOURCE 200809L
#include "request_type.h"

#include "debug.h"
#include "globals.h"
#include "settings.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void add_operation_to_trace(MPIOPT_Request *request, char *operation) {

  struct trace_list_elem *new_elem = malloc(sizeof(struct trace_list_elem));
  new_elem->elem = strdup(operation);
  new_elem->next = NULL;
  assert(request->debug_data->trace_list_tail->next == NULL);
  request->debug_data->trace_list_tail->next = new_elem;
  request->debug_data->trace_list_tail = new_elem;
#ifdef DUMP_DEBUG_TRACE_EVERY_TIME
  dump_trace_to_file(request);
#endif
}

char *get_trace_filename(MPIOPT_Request *request) {
  char *format_string = "%s.%d.to.%d.tag.%d.trace.out";
  char *mode = "Send_Request";
  if (is_recv_type(request))
    mode = "Recv_Request";
  int rank = -1;
  MPI_Comm_rank(request->communicators->original_communicator, &rank);
  size_t buf_sz = 1 + snprintf(NULL, 0, format_string, mode, rank,
                               request->dest, request->tag);
  char *buf = malloc(buf_sz);
  sprintf(buf, format_string, mode, rank, request->dest, request->tag);
  return buf;
}

void dump_trace_to_file(MPIOPT_Request *request) {
  char *fname = get_trace_filename(request);
  FILE *fp = fopen(fname, "w");
  struct trace_list_elem *elem = request->debug_data->trace_list_head;
  while (elem != NULL) {
    fprintf(fp, "%s\n", elem->elem);
    elem = elem->next;
  }
  fclose(fp);
  free(fname);
}

void init_debug_data(MPIOPT_Request *request) {
  request->debug_data = malloc(sizeof(struct debug_data));
  struct trace_list_elem *head_elem = malloc(sizeof(struct trace_list_elem));
  head_elem->next = NULL;
  head_elem->elem =
      strdup("Begin Trace: Init Request"); // TODO one could write some info
                                           // about the request here
  request->debug_data->trace_list_head = head_elem;
  request->debug_data->trace_list_tail = head_elem;
}

void free_debug_data(MPIOPT_Request *request) {
  struct trace_list_elem *elem = request->debug_data->trace_list_head;
  while (elem != NULL) {
    free(elem->elem);
    struct trace_list_elem *next = elem->next;
    free(elem);
    elem = next;
  }
  free(request->debug_data);
}

#endif
