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

// bufsize and num iter have to be large to get performance benefit, otherwise
// slowdown occur
// KB
// #define BUFFER_SIZE 1000
// MB
// #define BUFFER_SIZE 1000000

// 10KB
#define BUFFER_SIZE 10000
#define NUM_ITERS 10

// Count of blocks in non contiguous datatype
#define BLOCK_COUNT 10
#define BLOCK_SIZE 10

// #define BUFFER_SIZE 10
// #define NUM_ITERS 10

// #define N BUFFER_SIZE
#define N 3

void dummy_workload(double *buf) {

  for (int i = 0; i < N - 1; ++i) {
    buf[i] = sin(buf[i + 1]);
  }
}

void check_buffer_content(char *buf, int n, int *block_sizes) {
  int not_correct = 0;
  char sent_item = 0;
  int sent_item_count = 0;
  int block = -1;

  for (int i = 0; i < BLOCK_COUNT * BLOCK_SIZE * N; ++i) {

    if (sent_item && sent_item_count == block_sizes[block]) {
      sent_item = 0;
      sent_item_count = 0;
    }

    if (i % BLOCK_SIZE == 0) {
      sent_item = 1;
      block++;
      block %= BLOCK_COUNT;
    }

    if (sent_item) {
      sent_item_count++;

      if (buf[i] != 2 * (n + 1)) {
        not_correct++;
        printf("position %d: %d exptected %d\n", i, buf[i], 2 * (n + 1));
      }
    } else {
      if (buf[i] != n + 1) {
        not_correct++;
        printf("position %d: %d exptected %d\n", i, buf[i], n + 1);
      }
    }
  }

  if (not_correct != 0) {
    printf(
        "ERROR: %d: buffer has unexpected content (counted %d wrong values)\n",
        n, not_correct);
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
  char *buffer = malloc(BLOCK_COUNT * BLOCK_SIZE * N * sizeof(char));
  double *work_buffer = calloc(N, sizeof(double));
  work_buffer[N - 1] = 0.6;

  // create non contiguous datatype
  // blocklengths cycle through 1 ... 5
  // displacements is BLOCK_SIZE * blockindex at all times (could use another
  // datatype constructer in this case)
  // -> [a, b, b, ..., a, a, b, b, ..., a, a, a, b, ...] where a is a sent entry
  // and b is a not sent entry. full array has size BLOCK_COUNT * 10

  int *block_lenghts = malloc(BLOCK_COUNT * sizeof(int));
  int *displacements = malloc(BLOCK_COUNT * sizeof(int));

  for (int i = 0; i < BLOCK_COUNT; ++i) {
    block_lenghts[i] = (i % 5) + 1;
    displacements[i] = BLOCK_SIZE * i;
  }

  block_lenghts[BLOCK_COUNT - 1] = BLOCK_SIZE;

  MPI_Datatype nc_datatype;

  MPI_Type_indexed(BLOCK_COUNT, block_lenghts, displacements, MPI_CHAR,
                   &nc_datatype);
  MPI_Type_commit(&nc_datatype);


  MPI_Request req;

  if (rank == 1) {

    MPI_Send_init(buffer, N, nc_datatype, 0, 42, MPI_COMM_WORLD, &req);

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

    MPI_Recv_init(buffer, N, nc_datatype, 1, 42, MPI_COMM_WORLD, &req);
    for (int n = 0; n < NUM_ITERS; ++n) {

      for (int i = 0; i < BLOCK_COUNT * BLOCK_SIZE * N; ++i) {
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
