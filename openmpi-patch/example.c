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

#define DUMMY_WLOAD_TIME = 10

#define STATISTIC_PRINTING

// bufsize and num iter have to be large to get performance benefit, otherwise
// slowdown occur
// KB
//#define BUFFER_SIZE 1000
// MB
//#define BUFFER_SIZE 1000000

// 10KB
#define BUFFER_SIZE 10000
#define NUM_ITERS 3

// Count of blocks in non contiguous datatype
#define BLOCK_COUNT 2
#define BLOCK_SIZE 5

//#define BUFFER_SIZE 10
//#define NUM_ITERS 10

//#define N BUFFER_SIZE
#define N 2

void dummy_workload(double *buf) {

  for (int i = 0; i < N - 1; ++i) {
    buf[i] = sin(buf[i + 1]);
  }
}

void check_buffer_content(char *buf, int n, int* block_sizes) {
  int not_correct = 0;
  char sent_item = 0;
  int sent_item_count = 0;
  int block = -1;

  for (int i = 0; i < BLOCK_COUNT * BLOCK_SIZE * N; ++i) {

    if(sent_item && sent_item_count == block_sizes[block]) {
      sent_item = 0;
      sent_item_count = 0;
    }

    if(i % BLOCK_SIZE == 0){
      sent_item = 1;
      block++;
      block %= BLOCK_COUNT;
    }

    if(sent_item){
      sent_item_count++;
      
      if(buf[i] != 2 * (n + 1)){
        not_correct++;
        printf("position %d: %d exptected %d\n", i, buf[i], 2 * (n+1));
      }
    } else {
      if(buf[i] != n + 1) {
        not_correct++;
        printf("position %d: %d exptected %d\n", i, buf[i], n+1);
      }
    }
  }

  if (not_correct != 0) {
    printf("ERROR: %d: buffer has unexpected content (counted %d wrong values)\n", n, not_correct);
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
  char *buffer = malloc(BLOCK_COUNT * BLOCK_SIZE * N);
  double *work_buffer = calloc(N, sizeof(double));
  work_buffer[N - 1] = 0.6;

  // create non contiguous datatype
  // blocklengths cycle through 1 ... 5
  // displacements is BLOCK_SIZE * blockindex at all times (could use another datatype constructer in this case)
  // -> [a, b, b, ..., a, a, b, b, ..., a, a, a, b, ...] where a is a sent entry and b is a not sent entry.
  // full array has size BLOCK_COUNT * 10

  // size = size of all sent elements
  // extent = size of sent and unsent elements

  // MPI represents any created type as a map with {(type0, displ0), (type1, displ1), ..., (typeN-1, displN-1)}

  int *block_lenghts = malloc(BLOCK_COUNT * sizeof(int));
  int *displacements = malloc(BLOCK_COUNT * sizeof(int));

  for(int i = 0; i < BLOCK_COUNT; ++i){
    block_lenghts[i] = (i % 5) + 1;
    //block_lenghts[i] = 2;
    displacements[i] = BLOCK_SIZE*i;
  }

  block_lenghts[BLOCK_COUNT - 1] = BLOCK_SIZE;

  MPI_Datatype nc_datatype;

  MPI_Type_indexed(BLOCK_COUNT, block_lenghts, displacements, MPI_BYTE, &nc_datatype);
  MPI_Type_commit(&nc_datatype);

  MPI_Count typesize, lb, extent;
  MPI_Type_size_x(nc_datatype, &typesize);
  MPI_Type_get_extent_x(nc_datatype, &lb, &extent);

  //int num_integers, num_adresses, num_datatypes, combiner;
  //MPI_Type_get_envelope(nc_datatype, &num_integers, &num_adresses, &num_datatypes, &combiner);

  //int *array_of_ints = malloc(num_integers * sizeof(int));
  //MPI_Aint *array_of_addresses = malloc(num_adresses * sizeof(MPI_Aint));
  //MPI_Datatype *array_of_datatypes = malloc(num_datatypes * sizeof(MPI_Datatype));

  //MPI_Type_get_contents(nc_datatype, num_integers, num_adresses, num_datatypes, array_of_ints, array_of_addresses, array_of_datatypes);
#ifdef STATISTIC_PRINTING
  printf("Typesize: %d, Type extent: %d\n", typesize, extent);
#endif

  MPI_Request req;

  if (rank == 1) {

    MPIOPT_Send_init(buffer, N, nc_datatype, 0, 42, MPI_COMM_WORLD, &req);

    for (int n = 0; n < NUM_ITERS; ++n) {
      for (int i = 0; i < BLOCK_COUNT * BLOCK_SIZE * N; ++i) {
        //buffer[i] = rank * i * n;
        buffer[i] = 2 * (n + 1);
      }

      printf("Send %d\n", n);
      MPIOPT_Start(&req);
      dummy_workload(work_buffer);
      MPIOPT_Wait(&req, MPI_STATUS_IGNORE);
      /*int flag = 0;
      while (!flag) {
        MPIOPT_Test(&req, &flag, MPI_STATUS_IGNORE);
      }
      */
    }
  } else {

    MPIOPT_Recv_init(buffer, N, nc_datatype, 1, 42, MPI_COMM_WORLD, &req);
    for (int n = 0; n < NUM_ITERS; ++n) {

      for (int i = 0; i < BLOCK_COUNT * BLOCK_SIZE * N; ++i) {
        //buffer[i] = rank * i * n;
        buffer[i] = (n + 1);
      }

      printf("Recv %d\n", n);
      MPIOPT_Start(&req);
      dummy_workload(work_buffer);
      MPIOPT_Wait(&req, MPI_STATUS_IGNORE);
      // int flag=0;
      // while (! flag){
      //     MPIOPT_Test(&req,&flag, MPI_STATUS_IGNORE);
      //}
#ifdef STATISTIC_PRINTING
      check_buffer_content(buffer, n, block_lenghts);
#endif
    }

    // after comm
    /*
     for (int i = 0; i < N; ++i) {
     printf("%i,", buffer[i]);
     }
     printf("\n");
     */
  }

  MPI_Type_free(&nc_datatype);

  MPIOPT_Request_free(&req);
  MPIOPT_FINALIZE();
}

void use_standard_comm() {

  int rank, numtasks;
  // Welchen rang habe ich?
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  // wie viele Tasks gibt es?
  MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
  char *buffer = malloc(BLOCK_COUNT * BLOCK_SIZE * N);
  double *work_buffer = calloc(N, sizeof(double));
  work_buffer[N - 1] = 0.6;

  // create nc Datatype
  int *block_lenghts = malloc(BLOCK_COUNT * sizeof(int));
  int *displacements = malloc(BLOCK_COUNT * sizeof(int));

  for(int i = 0; i < BLOCK_COUNT; ++i){
    block_lenghts[i] = (i % 5) + 1;
    //block_lenghts[i] = 2;
    displacements[i] = BLOCK_SIZE*i;
  }

  block_lenghts[BLOCK_COUNT - 1] = BLOCK_SIZE;

  MPI_Datatype nc_datatype;

  MPI_Type_indexed(BLOCK_COUNT, block_lenghts, displacements, MPI_BYTE, &nc_datatype);
  MPI_Type_commit(&nc_datatype);

  MPI_Count typesize, lb, extent;
  MPI_Type_size_x(nc_datatype, &typesize);
  MPI_Type_get_extent_x(nc_datatype, &lb, &extent);

  //int num_integers, num_adresses, num_datatypes, combiner;
  //MPI_Type_get_envelope(nc_datatype, &num_integers, &num_adresses, &num_datatypes, &combiner);

  //int *array_of_ints = malloc(num_integers * sizeof(int));
  //MPI_Aint *array_of_addresses = malloc(num_adresses * sizeof(MPI_Aint));
  //MPI_Datatype *array_of_datatypes = malloc(num_datatypes * sizeof(MPI_Datatype));

  //MPI_Type_get_contents(nc_datatype, num_integers, num_adresses, num_datatypes, array_of_ints, array_of_addresses, array_of_datatypes);

#ifdef STATISTIC_PRINTING
  printf("Typesize: %d, Type extent: %d\n", typesize, extent);
#endif


  MPI_Request req;

  if (rank == 1) {

    for (int n = 0; n < NUM_ITERS; ++n) {
      for (int i = 0; i < BLOCK_COUNT * BLOCK_SIZE * N; ++i) {
        //buffer[i] = rank * i * n;
        buffer[i] = 2 * (n + 1);
      }

      MPI_Isend(buffer, N, nc_datatype, 0, 42, MPI_COMM_WORLD, &req);
      dummy_workload(work_buffer);
      MPI_Wait(&req, MPI_STATUS_IGNORE);
    }
  } else {
    for (int n = 0; n < NUM_ITERS; ++n) {
      for (int i = 0; i < BLOCK_COUNT * BLOCK_SIZE * N; ++i) {
        //buffer[i] = rank * i * n;
        buffer[i] = n + 1;
      }

      MPI_Irecv(buffer, N, nc_datatype, 1, 42, MPI_COMM_WORLD, &req);
      dummy_workload(work_buffer);
      MPI_Wait(&req, MPI_STATUS_IGNORE);
#ifdef STATISTIC_PRINTING
      check_buffer_content(buffer, n, block_lenghts);
#endif
    }

    // after comm
    /*
     for (int i = 0; i < N; ++i) {
     printf("%i,", buffer[i]);
     }
     printf("\n");
     */
  }
  MPI_Type_free(&nc_datatype);
}

void use_persistent_comm() {

  int rank, numtasks;
  // Welchen rang habe ich?
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  // wie viele Tasks gibt es?
  MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
  int *buffer = malloc(N * sizeof(int));
  double *work_buffer = calloc(N, sizeof(double));
  work_buffer[N - 1] = 0.6;

  MPI_Request req;

  if (rank == 1) {

    MPI_Send_init(buffer, N, MPI_INT, 0, 42, MPI_COMM_WORLD, &req);

    for (int n = 0; n < NUM_ITERS; ++n) {
      for (int i = 0; i < N; ++i) {
        buffer[i] = rank * i * n;
      }
      MPI_Start(&req);
      dummy_workload(work_buffer);
      MPI_Wait(&req, MPI_STATUS_IGNORE);
    }
  } else {

    MPI_Recv_init(buffer, N, MPI_INT, 1, 42, MPI_COMM_WORLD, &req);
    for (int n = 0; n < NUM_ITERS; ++n) {

      for (int i = 0; i < N; ++i) {
        buffer[i] = rank * i * n;
      }

      MPI_Start(&req);
      dummy_workload(work_buffer);
      MPI_Wait(&req, MPI_STATUS_IGNORE);
#ifdef STATISTIC_PRINTING
      //check_buffer_content(buffer, n);
#endif
    }

    // after comm
    /*
     for (int i = 0; i < N; ++i) {
     printf("%i,", buffer[i]);
     }
     printf("\n");
     */
  }

  MPI_Request_free(&req);
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
  MPI_Barrier(MPI_COMM_WORLD);
  gettimeofday(&start_time, NULL); /*  start timer         */
  use_standard_comm();
  gettimeofday(&stop_time, NULL); /*  stop timer          */
  time = (stop_time.tv_sec - start_time.tv_sec) +
         (stop_time.tv_usec - start_time.tv_usec) * 1e-6;

  printf("Standard:    %f s \n", time);
  MPI_Barrier(MPI_COMM_WORLD);
  gettimeofday(&start_time, NULL); /*  start timer         */
  //use_persistent_comm();
  gettimeofday(&stop_time, NULL); /*  stop timer          */
  time = (stop_time.tv_sec - start_time.tv_sec) +
         (stop_time.tv_usec - start_time.tv_usec) * 1e-6;

  printf("Persistent:    %f s \n", time);

  MPI_Finalize();
  return 0;
}
