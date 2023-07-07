#include "initialization.h"
#include "globals.h"
#include "handshake.h"
#include "interface.h"
#include "request_type.h"
#include "settings.h"

#include "debug.h"
#include "mpi-internals.h"

// datatype include
#include "opal/datatype/opal_datatype_internal.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// dummy int to create some mpi win on
int dummy_int = 0;

void MPIOPT_INIT() {
  // create the global win used for rdma transfers
  // TODO maybe we need less initialization to initialize the RDMA component?
  MPI_Win_create(&dummy_int, sizeof(int), 1, MPI_INFO_NULL, MPI_COMM_WORLD,
                 &global_comm_win);
  request_list_head = malloc(sizeof(struct list_elem));
  request_list_head->elem = NULL;
  request_list_head->next = NULL;
  to_free_list_head = malloc(sizeof(struct list_elem));
  to_free_list_head->elem = NULL;
  to_free_list_head->next = NULL;

  communicator_array =
      malloc(sizeof(struct communicator_info) * MAX_NUM_OF_COMMUNICATORS);

#ifdef SUMMARY_STATISTIC_PRINTING
  crosstalk_counter = 0;
#endif
  MPIOPT_Register_Communicator(MPI_COMM_WORLD);
}

struct communicator_info *find_comm(MPI_Comm comm) {
  for (int i = 0; i < communicator_array_size; ++i) {
    if (communicator_array[i].original_communicator == comm)
      return &communicator_array[i];
  }

#ifdef REGISTER_COMMUNICATOR_ON_USE
  MPIOPT_Register_Communicator(comm);
  for (int i = 0; i < communicator_array_size; ++i) {
    if (communicator_array[i].original_communicator == comm)
      return &communicator_array[i];
  }
#endif

  assert(false && "Communicator was not registered");
  return NULL;
}

#ifdef CHECK_FOR_MATCHING_CONFLICTS
LINKAGE_TYPE int check_for_conflicting_request(MPIOPT_Request *request) {

  struct list_elem *current = request_list_head->next;
  while (current != NULL) {
    MPIOPT_Request *other = current->elem;
    assert(other != NULL);
    if (other != request) {
      // same communication direction
      if ((is_sending_type(request) && is_sending_type(other)) ||
          (is_recv_type(request) && is_recv_type(other))) {
        // same envelope
        if (request->dest == other->dest && request->tag == other->tag &&
            request->communicators->original_communicator ==
                other->communicators->original_communicator) {
          assert(false &&
                 "Requests with a matching envelope are not permitted");
          return 1;
        }
      }
    }
    current = current->next;
  }
  return 0;
}
#endif

int get_opal_element_type_size(uint16_t type) {
  // see opal/datatype/opal_datatype_internal.h for the definitions
  switch (type) {
  case 4:
  case 9:
  case 22:
    return 1;

  case 5:
  case 10:
  case 14:
    return 2;

  case 6:
  case 11:
  case 15:
  case 23:
    return 4;

  case 7:
  case 12:
  case 16:
  case 19:
    return 8;

  case 17:
    return 12;

  case 8:
  case 13:
  case 18:
  case 20:
    return 16;

  case 21:
    return 32;

  default:
    printf("unrecognized type\n");
    return -1;
  }
}

void read_internal_opal_dtype(MPIOPT_Request *request) {
  opal_datatype_t *internal_dtype = &(request->dtype->super);

  int num_elems = internal_dtype->desc.used;

  int *unrolled_sizes = malloc(internal_dtype->nbElems);
  int *unrolled_disps = malloc(internal_dtype->nbElems);

  int *loop_stack = malloc(internal_dtype->loops / 2); // number of loops
  int *loop_count_stack = malloc(internal_dtype->loops / 2);
  int *loop_disp_stack = malloc(internal_dtype->loops / 2);
  int stack_top = -1;

  int current_element = 0;
  int unrolled_index = 0;

  while (current_element < num_elems) {
    uint16_t elem_type =
        internal_dtype->desc.desc[current_element].elem.common.type;
    if (elem_type == 0) {
      // element is a loop start
      ddt_loop_desc_t *loop_start =
          &internal_dtype->desc.desc[current_element].loop;

      stack_top++;
      loop_stack[stack_top] = current_element;
      loop_count_stack[stack_top] = loop_start->loops;
      loop_disp_stack[stack_top] = 0;
      current_element++;

    } else if (elem_type == 1) {
      // element is a loop end
      ddt_endloop_desc_t *loop_end =
          &internal_dtype->desc.desc[current_element].end_loop;

      if (loop_count_stack[stack_top] > 1) {
        // loop still going
        loop_count_stack[stack_top]--;
        current_element = loop_stack[stack_top] + 1;
        loop_disp_stack[stack_top] +=
            internal_dtype->desc.desc[current_element - 1].loop.extent;
      } else {
        stack_top--;
        current_element++;
      }

    } else {
      // element is an actual type
      ddt_elem_desc_t *element =
          &internal_dtype->desc.desc[current_element].elem;
      int element_size = get_opal_element_type_size(element->common.type);

      for (int k = 0; k < element->count; ++k) {

        int loop_offset = 0;
        for (int i = 0; i <= stack_top; ++i) {
          loop_offset += loop_disp_stack[i];
        }

        unrolled_sizes[unrolled_index] = element_size * element->blocklen;
        unrolled_disps[unrolled_index] =
            element->disp + k * element->extent + loop_offset;
        unrolled_index++;
      }

      current_element++;
    }
  }

  // print unoptimized typemap
  /*for(int i = 0; i < unrolled_index; ++i) {
    printf("(%d, %d), ", unrolled_disps[i], unrolled_sizes[i]);
  }
  printf("\n");*/
  // printf("extent %d\n", request->dtype_extent);

  int *unrolled_count_sizes =
      malloc(unrolled_index * request->count * sizeof(int));
  int *unrolled_count_disps =
      malloc(unrolled_index * request->count * sizeof(int));
  for (int k = 0; k < request->count; ++k) {
    for (int i = 0; i < unrolled_index; ++i) {
      unrolled_count_disps[k * unrolled_index + i] =
          unrolled_disps[i] + k * request->dtype_extent;
      unrolled_count_sizes[k * unrolled_index + i] = unrolled_sizes[i];
    }
  }

  // optimize type map by merging contiguous blocks
  request->num_cont_blocks = 1;
  int current_opt_elem = 0;
  for (int i = 1; i < unrolled_index * request->count; ++i) {
    if (unrolled_count_disps[current_opt_elem] +
            unrolled_count_sizes[current_opt_elem] ==
        unrolled_count_disps[i]) {
      // next element comes directly after current contiguous block
      // increase size instead of creating new element
      unrolled_count_sizes[current_opt_elem] += unrolled_count_sizes[i];
    } else {
      // next element is not directly after current block
      // start new contiguous block
      current_opt_elem++;
      unrolled_count_disps[current_opt_elem] = unrolled_count_disps[i];
      unrolled_count_sizes[current_opt_elem] = unrolled_count_sizes[i];
      request->num_cont_blocks++;
    }
  }

  request->dtype_displacements = malloc(request->num_cont_blocks * sizeof(int));
  request->dtype_lengths = malloc(request->num_cont_blocks * sizeof(int));
  request->pack_size = 0;
  for (int i = 0; i < request->num_cont_blocks; ++i) {
    request->dtype_displacements[i] = unrolled_count_disps[i];
    request->dtype_lengths[i] = unrolled_count_sizes[i];
    request->pack_size += unrolled_count_sizes[i];
  }

  // print typemap
  /*printf("num_cont_blocks %d\n", request->num_cont_blocks);
  for(int i = 0; i < request->num_cont_blocks; ++i) {
    printf("(%d, %d), ", request->dtype_displacements[i],
  request->dtype_lengths[i]);
  }
  printf("\n");*/

  free(unrolled_disps);
  free(unrolled_sizes);
  free(loop_stack);
  free(loop_count_stack);
  free(loop_disp_stack);
  free(unrolled_count_sizes);
  free(unrolled_count_disps);
}

LINKAGE_TYPE int init_request(const void *buf, int count, MPI_Datatype datatype,
                              int dest, int tag, MPI_Comm comm,
                              MPIOPT_Request *request, bool is_send_request,
                              MPI_Info info) {

  MPI_Count type_size, type_extent, lb;
  MPI_Type_size_x(datatype, &type_size);
  MPI_Type_get_extent_x(datatype, &lb, &type_extent);
#ifndef NDEBUG
  // only if assertion checking is on
#endif
  // is contigous
  // assert(type_size == type_extent && lb == 0 &&
  //       "Only contigous datatypes are supported yet");
  assert(type_size != MPI_UNDEFINED);

  int rank, numtasks;
  // Welchen rang habe ich?
  MPI_Comm_rank(comm, &rank);
  // wie viele Tasks gibt es?
  MPI_Comm_size(comm, &numtasks);

  void *buffer_ptr = buf;

  ompi_osc_ucx_module_t *module =
      (ompi_osc_ucx_module_t *)global_comm_win->w_osc_module;
  ucp_ep_h ep = OSC_UCX_GET_EP(module->comm, dest);

  request->ep = ep;
  request->is_cont = (type_size == type_extent);
  request->size = type_size * count;
  request->count = count;
  request->dtype_extent = type_extent;

  request->dtype = datatype;

  request->buf = buf;

  if (!(request->is_cont)) {
    char info_send_strategy[MPI_MAX_INFO_VAL];
    int info_flag;

    if (info != MPI_INFO_NULL) {
      MPI_Info_get(info, "nc_send_strategy", MPI_MAX_INFO_VAL,
                   info_send_strategy, &info_flag);
    } else {
      info_flag = 0;
    }
    if (info_flag) {
      if (strcmp(info_send_strategy, "PACK") == 0) {
        request->nc_strategy = NC_PACKING;
      } else if (strcmp(info_send_strategy, "DIRECT_SEND") == 0) {
        request->nc_strategy = NC_DIRECT_SEND;
      } else if (strcmp(info_send_strategy, "MIXED") == 0) {
        request->nc_strategy = NC_MIXED;
        int threshold_flag = 0;
        char info_threshold[MPI_MAX_INFO_VAL];

        MPI_Info_get(info, "nc_mixed_threshold", MPI_MAX_INFO_VAL,
                     info_threshold, &threshold_flag);
        if (threshold_flag) {
          request->threshold = atoi(info_threshold);
          if (request->threshold == 0) {
            request->threshold = DEFAULT_THRESHOLD;
          }
        } else {
          request->threshold = DEFAULT_THRESHOLD;
        }
      } else if (strcmp(info_send_strategy, "OPT_PACKING") == 0) {
        request->nc_strategy = NC_OPT_PACKING;
      } else {
        printf("Unrecognized sending strategy. Using PACK as fallback.\n");
        request->nc_strategy = NC_PACKING;
      }
    } else {
      printf("No sending strategy provided. Using PACK as fallback\n");
      request->nc_strategy = NC_PACKING;
    }

    switch (request->nc_strategy) {
    case NC_PACKING:
      // PACKING

      printf("using packing strategy\n");

      MPI_Pack_size(count, datatype, comm, &request->pack_size);
      request->packed_buf = malloc(request->pack_size);

      break;

    case NC_DIRECT_SEND:
      // DIRECT SEND
      printf("using direct send strategy\n");
      read_internal_opal_dtype(request);

      break;

    case NC_OPT_PACKING:
      printf("using optimized packing strategy\n");
      read_internal_opal_dtype(request);
      request->packed_buf = malloc(request->pack_size);
      break;

    case NC_MIXED:
      printf("using mixed strategy with threshold %d bytes\n",
             request->threshold);
      read_internal_opal_dtype(request);
      request->pack_size = 0;
      for (int i = 0; i < request->num_cont_blocks; i++) {
        if (request->dtype_lengths[i] <= request->threshold) {
          request->pack_size += request->dtype_lengths[i];
        }
      }
      request->packed_buf = malloc(request->pack_size);
      break;

    default:
      break;
    }
  }

  request->original_request.req_type = MPIOPT_REQUEST_TYPE;

  request->dest = dest;
  request->dtype_size = type_size;
  request->tag = tag;
  request->backup_request = MPI_REQUEST_NULL;
  request->remote_data_addr = 0; // NULL

  request->communicators = find_comm(comm);
  assert(request->communicators != NULL);

  request->operation_number = 0;
  request->flag = 2; // operation 0 is completed

#ifndef NDEBUG
  init_debug_data(request);
#endif

#ifdef BUFFER_CONTENT_CHECKING
  // use c alloc, so that it is initialized, even if a smaller msg was received
  // to avoid undefined behaviour
  request->checking_buf = calloc(request->size, 1);
  request->chekcking_request = MPI_REQUEST_NULL;
#endif

  int conflicts = 0;
#ifdef CHECK_FOR_MATCHING_CONFLICTS
  conflicts = check_for_conflicting_request(request);
#endif

#ifdef USE_FALLBACK_UNTIL_THRESHOLD
  const bool use_fallback = request->size < FALLBACK_THRESHOLD;
#ifdef SUMMARY_STATISTIC_PRINTING
  if (use_fallback)
    printf("Use Fallback for small msg\n");
#endif
#else
  const bool use_fallback = 0;
#endif

  if (use_fallback || rank == dest || rank == MPI_PROC_NULL || conflicts) {
    // use the default implementation for communication with self / no-op
    if (!is_send_request) {
      set_request_type(request, RECV_REQUEST_TYPE_USE_FALLBACK);
    } else {
      set_request_type(request, SEND_REQUEST_TYPE_USE_FALLBACK);
    }
  } else {
    if (!is_send_request) {
      set_request_type(request, RECV_REQUEST_TYPE_HANDSHAKE_NOT_STARTED);
    } else {
      set_request_type(request, SEND_REQUEST_TYPE_HANDSHAKE_NOT_STARTED);
    }
  }
  // add request to list, so that it is progressed, if other requests have to
  // wait
  add_request_to_list(request);

  return MPI_SUCCESS;
}

LINKAGE_TYPE int MPIOPT_Recv_init_internal(void *buf, int count,
                                           MPI_Datatype datatype, int source,
                                           int tag, MPI_Comm comm,
                                           MPIOPT_Request *request,
                                           MPI_Info info) {
  memset(request, 0, sizeof(MPIOPT_Request));
  return init_request(buf, count, datatype, source, tag, comm, request, false,
                      info);
}

LINKAGE_TYPE int MPIOPT_Send_init_internal(const void *buf, int count,
                                           MPI_Datatype datatype, int source,
                                           int tag, MPI_Comm comm,
                                           MPIOPT_Request *request,
                                           MPI_Info info) {
  memset(request, 0, sizeof(MPIOPT_Request));
  return init_request(buf, count, datatype, source, tag, comm, request, true,
                      info);
}
