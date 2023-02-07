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

/* ************************************************************************ */
/*  main                                                                    */
/* ************************************************************************ */

//#define STATISTIC_PRINTING
#define READY_TO_RECEIVE 1
#define READY_TO_SEND 2

#define SEND 3
#define RECEIVED 4

#define NUM_REQUESTS 10

#define DUMMY_WLOAD_TIME 10

//#define STATISTIC_PRINTING

// bufsize and num iter have to be large to get performance benefit, otherwise
// slowdown occur
// KB
//#define BUFFER_SIZE 1000
// MB
//#define BUFFER_SIZE 1000000

// 10KB
#define BUFFER_SIZE 10000
#define NUM_ITERS 100000

//#define BUFFER_SIZE 10
//#define NUM_ITERS 10

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

void use_self_implemented_comm() {

  MPIOPT_INIT();

  int rank, numtasks;
  // Welchen rang habe ich?
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  // wie viele Tasks gibt es?
  MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
  int *buffer = malloc(N * NUM_REQUESTS * sizeof(int));
  double *work_buffer = calloc(N, sizeof(double));
  work_buffer[N - 1] = 0.6;

  MPI_Request reqs[NUM_REQUESTS];

  for (int i = 0; i < NUM_REQUESTS; ++i) {
    if (rank == 1) {
      MPIOPT_Send_init(&buffer[i * N], N, MPI_INT, 0, 42 + i, MPI_COMM_WORLD,
                       &reqs[i]);
    } else {
      MPIOPT_Recv_init(&buffer[i * N], N, MPI_INT, 1, 42 + i, MPI_COMM_WORLD,
                       &reqs[i]);
    }
  }

  for (int n = 0; n < NUM_ITERS; ++n) {
    for (int i = 0; i < N * NUM_REQUESTS; ++i) {
      buffer[i] = rank * i * n;
    }
    for (int i = 0; i < NUM_REQUESTS; ++i) {
      MPIOPT_Start(&reqs[i]);
    }
    dummy_workload(work_buffer);
    for (int i = 0; i < NUM_REQUESTS; ++i) {
      MPIOPT_Wait(&reqs[i], MPI_STATUS_IGNORE);
    }
    /*int flag = 0;
    while (!flag) {
      MPIOPT_Test(&reqs, &flag, MPI_STATUS_IGNORE);
    }*/
  }

  for (int i = 0; i < NUM_REQUESTS; ++i) {
    MPIOPT_Request_free(&reqs[i]);
  }
  MPIOPT_FINALIZE();
}

int main(int argc, char **argv) {

  struct timeval start_time; /* time when program started */
  struct timeval stop_time; /* time when calculation completed                */

  // Initialisiere Alle Prozesse
  MPI_Init(&argc, &argv);

  gettimeofday(&start_time, NULL); /*  start timer         */
  use_self_implemented_comm();
  gettimeofday(&stop_time, NULL); /*  stop timer          */
  double time = (stop_time.tv_sec - start_time.tv_sec) +
                (stop_time.tv_usec - start_time.tv_usec) * 1e-6;

  printf("Self Implemented:    %f s \n", time);

  MPI_Finalize();
  return 0;
}
