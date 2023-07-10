#include <inttypes.h>
#include <malloc.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "mpi.h"

#include <execinfo.h>

#include <math.h>

/* ************************************************************************ */
/*  main                                                                    */
/* ************************************************************************ */

#define DUMMY_WLOAD_TIME = 10

// 10KB
#define BUFFER_SIZE 10000
#define NUM_ITERS 1000

#define N BUFFER_SIZE

void dummy_workload(double *buf) {

  for (int i = 0; i < N - 1; ++i) {
    buf[i] = sin(buf[i + 1]);
  }
}

void check_buffer_content(int *buf, int n) {
  int not_correct = 0;

  for (int i = 0; i < N; ++i) {
    if (buf[i] != 1 * i * n) {
      not_correct++;
    }
  }

  if (not_correct != 0) {
    printf("ERROR: %d: buffer has unexpected content\n", n);
    // exit(-1);
  }
}

#define tag_entry 42
#define tag_rkey_data 43
#define tag_rkey_flag 44

void use_persistent_comm() {

  MPI_Comm comm;
  MPI_Comm_dup(MPI_COMM_WORLD, &comm);

  int rank, numtasks;
  // Welchen rang habe ich?
  MPI_Comm_rank(comm, &rank);
  // wie viele Tasks gibt es?
  MPI_Comm_size(comm, &numtasks);
  int *buffer = malloc(N * sizeof(int));
  double *work_buffer = calloc(N, sizeof(double));
  work_buffer[N - 1] = 0.6;

  MPI_Request req;

  if (rank == 1) {

    MPI_Send_init(buffer, N, MPI_INT, 0, 42, comm, &req);

    for (int n = 0; n < NUM_ITERS; ++n) {
      for (int i = 0; i < N; ++i) {
        buffer[i] = rank * i * n;
      }

      printf("Send %d\n", n);
      MPI_Start(&req);
      dummy_workload(work_buffer);
      MPI_Wait(&req, MPI_STATUS_IGNORE);
    }
  } else {

    MPI_Recv_init(buffer, N, MPI_INT, 1, 42, comm, &req);
    for (int n = 0; n < NUM_ITERS; ++n) {

      for (int i = 0; i < N; ++i) {
        buffer[i] = rank * i * n;
      }
      printf("Recv %d\n", n);
      MPI_Start(&req);
      dummy_workload(work_buffer);
      MPI_Wait(&req, MPI_STATUS_IGNORE);
      check_buffer_content(buffer, n);
    }
  }

  MPI_Request_free(&req);
}

int main(int argc, char **argv) {

  struct timeval start_time; /* time when program started */
  struct timeval stop_time; /* time when calculation completed                */

  // Initialisiere Alle Prozesse
  MPI_Init(&argc, &argv);

  gettimeofday(&start_time, NULL); /*  start timer         */
  use_persistent_comm();
  gettimeofday(&stop_time, NULL); /*  stop timer          */
  double time = (stop_time.tv_sec - start_time.tv_sec) +
                (stop_time.tv_usec - start_time.tv_usec) * 1e-6;

  printf("Time:    %f s \n", time);

  MPI_Finalize();
  return 0;
}
