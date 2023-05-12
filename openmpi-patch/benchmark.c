#define _GNU_SOURCE /* needed for some ompi internal headers*/

#include <inttypes.h>
#include <malloc.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "interface.h"

#include <execinfo.h>

#include <math.h>

#define WORK_BUFFER_SIZE 1000

#define VEC_FILL_RATIO 0.5
#define VEC_NUM_BLOCKS 10

#define INDEXED_NUM_BLOCKS 10

#define tag_entry 42

void dummy_workload(double *buf) {

  for (int i = 0; i < WORK_BUFFER_SIZE - 1; ++i) {
    buf[i] = sin(buf[i + 1]);
  }
}

// datatype create functions
void create_cont_data(MPI_Datatype *dtype, int size) {
  MPI_Type_contiguous(size, MPI_BYTE, dtype);
  MPI_Type_commit(dtype);
}

void create_vector_data(MPI_Datatype *dtype, int size) {

  int stride = size / VEC_NUM_BLOCKS;
  int blocklength = (int)((double)stride * VEC_FILL_RATIO);

  MPI_Type_vector(VEC_NUM_BLOCKS, blocklength, stride, MPI_BYTE, dtype);
  MPI_Type_commit(dtype);
}

void create_indexed_data(MPI_Datatype *dtype, int size) {
  int disps[INDEXED_NUM_BLOCKS];
  int lengths[INDEXED_NUM_BLOCKS];
  int blocksize = size / INDEXED_NUM_BLOCKS;
  for (int i = 0; i < INDEXED_NUM_BLOCKS; ++i) {
    disps[i] = blocksize * i;
    lengths[i] =
        (int)ceil(((double)i / (double)INDEXED_NUM_BLOCKS) * (double)blocksize);
  }
  MPI_Type_indexed(INDEXED_NUM_BLOCKS, lengths, disps, MPI_BYTE, dtype);
  MPI_Type_commit(dtype);
}

void create_struct_data(MPI_Datatype *dtype, int size) {}

void create_combined_data(MPI_Datatype *dtype, int size) {}

void use_one_sided_persistent(MPI_Datatype *dtype, int count, int size,
                              int num_iters, MPI_Info *info) {
  MPIOPT_INIT();

  int rank, numtasks;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

  char *buffer = malloc(size);
  double *work_buffer = calloc(WORK_BUFFER_SIZE, sizeof(double));
  work_buffer[WORK_BUFFER_SIZE - 1] = 0.6;

  MPI_Request req;

  if (rank == 1) {

    MPIOPT_Send_init_x(buffer, count, *dtype, 0, 42, MPI_COMM_WORLD, &req,
                       *info);

    for (int n = 0; n < num_iters; ++n) {
      for (int i = 0; i < size; ++i) {
        buffer[i] = 2 * (n + 1);
      }
      MPIOPT_Start(&req);
      dummy_workload(work_buffer);
      MPIOPT_Wait(&req, MPI_STATUS_IGNORE);
    }
  } else {

    MPIOPT_Recv_init_x(buffer, count, *dtype, 1, 42, MPI_COMM_WORLD, &req,
                       *info);

    for (int n = 0; n < num_iters; ++n) {
      for (int i = 0; i < size; ++i) {
        buffer[i] = (n + 1);
      }

      MPIOPT_Start(&req);
      dummy_workload(work_buffer);
      MPIOPT_Wait(&req, MPI_STATUS_IGNORE);
    }
  }

  MPIOPT_Request_free(&req);
  free(buffer);
  free(work_buffer);
  MPIOPT_FINALIZE();
}

void use_standard_comm(MPI_Datatype *dtype, int count, int size,
                       int num_iters) {
  int rank, numtasks;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

  char *buffer = malloc(size);
  double *work_buffer = calloc(WORK_BUFFER_SIZE, sizeof(double));
  work_buffer[WORK_BUFFER_SIZE - 1] = 0.6;

  MPI_Request req;

  if (rank == 1) {

    for (int n = 0; n < num_iters; ++n) {
      for (int i = 0; i < size; ++i) {
        buffer[i] = 2 * (n + 1);
      }

      MPI_Isend(buffer, count, *dtype, 0, 42, MPI_COMM_WORLD, &req);
      dummy_workload(work_buffer);
      MPI_Wait(&req, MPI_STATUS_IGNORE);
    }
  } else {
    for (int n = 0; n < num_iters; ++n) {
      for (int i = 0; i < size; ++i) {
        buffer[i] = (n + 1);
      }

      MPI_Irecv(buffer, count, *dtype, 1, 42, MPI_COMM_WORLD, &req);
      dummy_workload(work_buffer);
      MPI_Wait(&req, MPI_STATUS_IGNORE);
    }
  }
  free(buffer);
  free(work_buffer);
}

void use_persistent_comm(MPI_Datatype *dtype, int count, int size,
                         int num_iters) {
  int rank, numtasks;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

  char *buffer = malloc(size);
  double *work_buffer = calloc(WORK_BUFFER_SIZE, sizeof(double));
  work_buffer[WORK_BUFFER_SIZE - 1] = 0.6;

  MPI_Request req;
  if (rank == 1) {

    MPI_Send_init(buffer, count, *dtype, 0, 42, MPI_COMM_WORLD, &req);

    for (int n = 0; n < num_iters; ++n) {
      for (int i = 0; i < size; ++i) {
        buffer[i] = 2 * (n + 1);
      }

      MPI_Start(&req);
      dummy_workload(work_buffer);
      MPI_Wait(&req, MPI_STATUS_IGNORE);
    }
  } else {

    MPI_Recv_init(buffer, count, *dtype, 1, 42, MPI_COMM_WORLD, &req);
    for (int n = 0; n < num_iters; ++n) {
      for (int i = 0; i < size; ++i) {
        buffer[i] = (n + 1);
      }

      MPI_Start(&req);
      dummy_workload(work_buffer);
      MPI_Wait(&req, MPI_STATUS_IGNORE);
    }
  }

  MPI_Request_free(&req);
  free(buffer);
  free(work_buffer);
}

int main(int argc, char **argv) {
  int num_iters = 0;
  int size = 0;
  int count = 0;
  int threshold = 0;
  int strategy = 0;
  int data = 0;

  for (int i = 0; i < argc; ++i) {
    if (strcmp(argv[i], "--iters") == 0) {
      num_iters = atoi(argv[i + 1]);
      i++;
      continue;
    }

    if (strcmp(argv[i], "--size") == 0) {
      size = atoi(argv[i + 1]);
      i++;
      continue;
    }

    if (strcmp(argv[i], "--count") == 0) {
      count = atoi(argv[i + 1]);
      i++;
      continue;
    }

    if (strcmp(argv[i], "--threshold") == 0) {
      threshold = atoi(argv[i + 1]);
      i++;
      continue;
    }

    if (strcmp(argv[i], "--strategy") == 0) {
      strategy = atoi(argv[i + 1]);
      i++;
      continue;
    }

    if (strcmp(argv[i], "--data") == 0) {
      data = atoi(argv[i + 1]);
      i++;
      continue;
    }
  }

  struct timeval start_time;
  struct timeval stop_time;
  double time;

  MPI_Init(&argc, &argv);

  int rank, numtasks;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

  MPI_Info info;
  MPI_Info_create(&info);

  MPI_Datatype dtype;

  switch (strategy) {
  case 0:
    MPI_Info_set(info, "nc_send_strategy", "PACK");
    break;

  case 1:
    MPI_Info_set(info, "nc_send_strategy", "DIRECT_SEND");
    break;

  case 2:
    MPI_Info_set(info, "nc_send_strategy", "OPT_PACKING");
    break;

  case 3:
    MPI_Info_set(info, "nc_send_strategy", "MIXED");
    break;
  }

  switch (data) {
  case 0:
    create_cont_data(&dtype, size);
    break;

  case 1:
    create_vector_data(&dtype, size);
    break;

  case 2:
    create_indexed_data(&dtype, size);
    break;

  case 3:
    create_struct_data(&dtype, size);
    break;

  case 4:
    create_combined_data(&dtype, size);
  }

  char threshold_str[MPI_MAX_INFO_VAL];
  sprintf(threshold_str, "%d", threshold);
  MPI_Info_set(info, "nc_mixed_threshold", threshold_str);

  gettimeofday(&start_time, NULL); // TODO MPI_Wtime
  use_one_sided_persistent(&dtype, count, size * count, num_iters, &info);
  gettimeofday(&stop_time, NULL);

  time = (stop_time.tv_sec - start_time.tv_sec) +
         (stop_time.tv_usec - start_time.tv_usec) * 1e-6;

  if (rank == 0)
    printf("ONE SIDED PERSISTENT:\ndata %d, strategy %d, iterations %d, count "
           "%d and size %d: %lfs\n\n",
           data, strategy, num_iters, count, size, time);

  gettimeofday(&start_time, NULL);
  use_standard_comm(&dtype, count, size * count, num_iters);
  gettimeofday(&stop_time, NULL);

  time = (stop_time.tv_sec - start_time.tv_sec) +
         (stop_time.tv_usec - start_time.tv_usec) * 1e-6;

  if (rank == 0)
    printf("STANDARD:\ndata %d, strategy %d, iterations %d, count %d and size "
           "%d: %lfs\n\n",
           data, strategy, num_iters, count, size, time);

  gettimeofday(&start_time, NULL);
  use_persistent_comm(&dtype, count, size * count, num_iters);
  gettimeofday(&stop_time, NULL);

  time = (stop_time.tv_sec - start_time.tv_sec) +
         (stop_time.tv_usec - start_time.tv_usec) * 1e-6;

  if (rank == 0)
    printf("PERSISTENT:\ndata %d, strategy %d, iterations %d, count %d and "
           "size %d: %lfs\n\n",
           data, strategy, num_iters, count, size, time);

  MPI_Type_free(&dtype);
  MPI_Finalize();
}