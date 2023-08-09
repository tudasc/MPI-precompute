#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

double *DATA;

MPI_Request comm_requests[4];

int global_tag = 42;

void init_communication_static(int rank_param, int size_param {
  printf("Static\n");
  // static will ignore the given parameters
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  static int tag_to_use = 0;
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

void init_communication_global(int rank, int size) {
  printf("Global\n");
  int other = MPI_PROC_NULL;
  if (rank != 0) {
    other = rank - 1;
  }
  MPI_Send_init(&DATA[25], 25, MPI_DOUBLE, other, global_tag, MPI_COMM_WORLD,
                &comm_requests[0]);
  MPI_Recv_init(&DATA[0], 25, MPI_DOUBLE, other, global_tag, MPI_COMM_WORLD,
                &comm_requests[1]);

  other = MPI_PROC_NULL;
  if (rank != size - 1) {
    other = rank + 1;
  }
  MPI_Send_init(&DATA[75], 25, MPI_DOUBLE, other, global_tag, MPI_COMM_WORLD,
                &comm_requests[2]);
  MPI_Recv_init(&DATA[50], 25, MPI_DOUBLE, other, global_tag, MPI_COMM_WORLD,
                &comm_requests[3]);
  global_tag++;
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

void run_iteration() {
  return; }

void calculate() {
  char running = 1;
  int iter = 0;
  while (running) {
    begin_halo_receive();

    run_iteration();

    begin_halo_send();

    end_halo_exchange();

    iter++;
    if (iter > 5) {
      running = 0;
    }
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

int main(int argc, char *argv[]) {
  int rank, size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  DATA = malloc(sizeof(double) * 100);
  for (int i = 0; i < 100; i++) {
    DATA[i] = 42.0;
  }

  void (*init_communication)(int, int) = 0;

  if (argc == 3) {
    init_communication = init_communication_static;
  } else {
    init_communication = init_communication_global;
  }

  init_communication(rank, size);

  calculate();

  free_communication();

  MPI_Finalize();
}
