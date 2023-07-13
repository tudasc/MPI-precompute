#include <inttypes.h>
#include <malloc.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <execinfo.h>

#include <math.h>

#include <mpi.h>

/* ************************************************************************ */
/*  main                                                                    */
/* ************************************************************************ */

// 10KB
#define BUFFER_SIZE 10000
#define NUM_ITERS 10

#define W_BUFFER_SIZE 100

void dummy_workload(double *buf) {

  for (int i = 0; i < W_BUFFER_SIZE - 1; ++i) {
    buf[i] = sin(buf[i + 1]);
  }
}

void check_buffer_content(int *buf, int n) {
  int not_correct = 0;

  for (int i = 0; i < BUFFER_SIZE; ++i) {
    if (buf[i] != 1 * i * n) {
      not_correct++;
    }
  }

  if (not_correct != 0) {
    printf("ERROR: %d: buffer has unexpected content\n", n);
    exit(-1);
  }
}

#define TAG 42

void use_persistent_comm() {

  int rank, numtasks;
  // Welchen rang habe ich?
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  // wie viele Tasks gibt es?
  MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
  char *buffer = malloc(BUFFER_SIZE * sizeof(int));
  double *work_buffer = calloc(W_BUFFER_SIZE, sizeof(double));
  work_buffer[W_BUFFER_SIZE - 1] = 0.6;

  MPI_Datatype datatype = MPI_INT;

  MPI_Request req;

  if (rank == 1) {

    MPI_Send_init(buffer, BUFFER_SIZE, datatype, 0, 42, MPI_COMM_WORLD, &req);

    for (int n = 0; n < NUM_ITERS; ++n) {
      for (int i = 0; i < BLOCK_COUNT * BLOCK_SIZE * N; ++i) {
        // buffer[i] = rank * i * n;
        buffer[i] = 2 * (n + 1);
      }

      printf("Send %d\n", n);
      MPI_Start(&req);
      dummy_workload(work_buffer);
      MPI_Wait(&req, MPI_STATUS_IGNORE);
    }
  } else {

    MPI_Recv_init(buffer, BUFFER_SIZE, datatype, 1, 42, MPI_COMM_WORLD, &req);
    for (int n = 0; n < NUM_ITERS; ++n) {

      for (int i = 0; i < BUFFER_SIZE; ++i) {
        // buffer[i] = rank * i * n;
        buffer[i] = (n + 1);
      }

      printf("Recv %d\n", n);
      MPI_Start(&req);
      dummy_workload(work_buffer);
      MPI_Wait(&req, MPI_STATUS_IGNORE);
      check_buffer_content(buffer, n, block_lenghts);
    }
  }

  MPI_Type_free(&nc_datatype);

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

  printf("Time needed:    %f s \n", time);

  MPI_Finalize();
  return 0;
}
