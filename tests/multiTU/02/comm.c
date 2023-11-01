#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

extern double *DATA;

MPI_Request comm_requests[4];
int tag_to_use = 0;

void init_communication(int rank, int size) {
  int other = MPI_PROC_NULL;
  if (rank != 0) {
    other = rank - 1;
  }
  MPI_Send_init(&DATA[25], 25, MPI_DOUBLE, other, tag_to_use, MPI_COMM_WORLD,
                &comm_requests[0]);
  MPI_Recv_init(&DATA[0], 25, MPI_DOUBLE, other, tag_to_use, MPI_COMM_WORLD,
                &comm_requests[1]);

  other = MPI_PROC_NULL;
  if (rank != size - 1) {
    other = rank + 1;
  }
  MPI_Send_init(&DATA[75], 25, MPI_DOUBLE, other, tag_to_use, MPI_COMM_WORLD,
                &comm_requests[2]);
  MPI_Recv_init(&DATA[50], 25, MPI_DOUBLE, other, tag_to_use, MPI_COMM_WORLD,
                &comm_requests[3]);

  tag_to_use++;
}

void begin_halo_receive() {
  MPI_Start(&comm_requests[1]);
  MPI_Start(&comm_requests[3]);
}

void begin_halo_send() {
  MPI_Start(&comm_requests[0]);
  MPI_Start(&comm_requests[2]);
}

void end_halo_exchange() {
  // could also use MPI_waitall
  for (int i = 0; i < 4; ++i) {
    MPI_Wait(&comm_requests[i], MPI_STATUS_IGNORE);
  }
}

void free_communication() {
  /* for (int i = 0; i < 4; ++i) { */
  /*     if (comm_requests[i] != MPI_REQUEST_NULL) { */
  /*         MPI_Request_free(&comm_requests[i]); */
  /*     } */
  /* } */

  MPI_Request_free(&comm_requests[0]);
  MPI_Request_free(&comm_requests[1]);
  MPI_Request_free(&comm_requests[2]);
  MPI_Request_free(&comm_requests[3]);
}