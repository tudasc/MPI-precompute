#include <assert.h>
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

#define NUM_REQUESTS 4

#define DUMMY_WLOAD_TIME 10

// 10KB
#define BUFFER_SIZE 1000
#define NUM_ITERS 100

#define N BUFFER_SIZE

void dummy_workload(double *buf) {

  for (int i = 0; i < N - 1; ++i) {
    buf[i] = sin(buf[i + 1]);
  }
}

void check_buffer_content(int *buf, int n, int rank) {
  int not_correct = 0;

  for (int i = 0; i < N * NUM_REQUESTS; ++i) {
    if (buf[i] != 1 * (rank + 1) * i * n) {
      not_correct++;
    }
  }

  if (not_correct != 0) {
    printf("ERROR: %d: buffer has unexpected content\n", n);
    exit(-1);
  }
}

#define tag_entry 42
#define tag_rkey_data 43
#define tag_rkey_flag 44

void use_persistent_comm() {

  int rank, numtasks;
  // Welchen rang habe ich?
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  // wie viele Tasks gibt es?
  MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
  int *buffer = malloc(N * NUM_REQUESTS * sizeof(int));
  double *work_buffer = calloc(N, sizeof(double));
  work_buffer[N - 1] = 0.6;

  MPI_Request reqs[NUM_REQUESTS];

  int nxt = (rank + 1) % numtasks;
  int prev = (rank + numtasks - 1) % numtasks;

  /* assert(NUM_REQUESTS % 2 == 0); */

  // TODO fuse these loops for better redability
  for (int i = 0; i < NUM_REQUESTS / 2; ++i) {

    MPI_Send_init(&buffer[i * N], N, MPI_INT, nxt, 42 + i, MPI_COMM_WORLD,
                  &reqs[i]);
  }
  for (int i = NUM_REQUESTS / 2; i < NUM_REQUESTS; ++i) {
    int tag = 42 + i - (NUM_REQUESTS / 2);
    MPI_Recv_init(&buffer[i * N], N, MPI_INT, prev, tag, MPI_COMM_WORLD,
                  &reqs[i]);
  }

  for (int n = 0; n < NUM_ITERS; ++n) {
    for (int i = 0; i < N * NUM_REQUESTS; ++i) {
      buffer[i] = (rank + 1) * i * n;
    }

    MPI_Startall(NUM_REQUESTS, reqs);

    dummy_workload(work_buffer);

    MPI_Waitall(NUM_REQUESTS, reqs, MPI_STATUSES_IGNORE);
    check_buffer_content(buffer, n, prev);
  }

  for (int i = 0; i < NUM_REQUESTS; ++i) {
    MPI_Request_free(&reqs[i]);
  }
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
