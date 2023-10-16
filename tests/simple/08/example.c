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

void check_buffer_content(int *buf, int n) {
  int not_correct = 0;

  for (int i = 0; i < BUFFER_SIZE; ++i) {
    if (buf[i] != 2 * (n + 1)) {
      //  printf("%d vs %d\n",buf[i],2 * (n + 1));
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
  int *buffer = malloc(BUFFER_SIZE * sizeof(int));
  int *buffer2 = malloc(BUFFER_SIZE * sizeof(int));

  MPI_Datatype datatype = MPI_INT;

  MPI_Request req;
  MPI_Request req2;

  if (rank == 1) {

    MPI_Send_init(buffer, BUFFER_SIZE, datatype, 0, 42, MPI_COMM_WORLD, &req);
    MPI_Recv_init(buffer2, BUFFER_SIZE, datatype, 0, 42, MPI_COMM_WORLD, &req2);

    for (int n = 0; n < NUM_ITERS; ++n) {
      for (int i = 0; i < BUFFER_SIZE; ++i) {
        buffer[i] = 2 * (n + 1);
      }

      MPI_Start(&req2);
      MPI_Start(&req);
      MPI_Wait(&req2, MPI_STATUS_IGNORE);
      MPI_Wait(&req, MPI_STATUS_IGNORE);
      check_buffer_content(buffer2, n);
    }
  } else {

    MPI_Send_init(buffer, BUFFER_SIZE, datatype, 1, 42, MPI_COMM_WORLD, &req);
    MPI_Recv_init(buffer2, BUFFER_SIZE, datatype, 1, 42, MPI_COMM_WORLD, &req2);

    for (int n = 0; n < NUM_ITERS; ++n) {
      for (int i = 0; i < BUFFER_SIZE; ++i) {
        buffer[i] = 2 * (n + 1);
      }

      MPI_Start(&req2);
      MPI_Start(&req);
      MPI_Wait(&req2, MPI_STATUS_IGNORE);
      MPI_Wait(&req, MPI_STATUS_IGNORE);
      check_buffer_content(buffer2, n);
    }
  }

  MPI_Request_free(&req);
  MPI_Request_free(&req2);
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
