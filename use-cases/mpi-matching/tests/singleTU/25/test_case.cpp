#include <cassert>
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <string>
#include <vector>

double *DATA;

MPI_Request comm_requests[4];

int get_rank() {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  return rank;
}

int get_size() {
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  return size;
}

void init_communication(int tag_to_use1, int tag_to_use2) {
  int other = MPI_PROC_NULL;
  if (get_rank() != 0) {
    other = get_rank() - 1;
  }
  std::cout << "Message Tags to use: " << tag_to_use1 << " and " << tag_to_use2
            << "\n";
  MPI_Send_init(&DATA[25], 25, MPI_DOUBLE, other, tag_to_use1, MPI_COMM_WORLD,
                &comm_requests[0]);
  MPI_Recv_init(&DATA[0], 25, MPI_DOUBLE, other, tag_to_use2, MPI_COMM_WORLD,
                &comm_requests[1]);

  other = MPI_PROC_NULL;
  if (get_rank() != get_size() - 1) {
    other = get_rank() + 1;
  }
  MPI_Send_init(&DATA[75], 25, MPI_DOUBLE, other, tag_to_use2, MPI_COMM_WORLD,
                &comm_requests[2]);
  MPI_Recv_init(&DATA[50], 25, MPI_DOUBLE, other, tag_to_use1, MPI_COMM_WORLD,
                &comm_requests[3]);
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

void run_iteration() { return; }

void calculate() {
  for (int iter = 0; iter < 5; ++iter) {
    begin_halo_receive();

    run_iteration();

    begin_halo_send();

    end_halo_exchange();
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

  double value_to_use = 1.0;

  DATA = (double *)malloc(sizeof(double) * 100);
  for (int i = 0; i < 100; i++) {
    DATA[i] = value_to_use;
  }

  init_communication(42, 42 + 42);

  std::cout << "Start Calculation on Rank " << get_rank() << "\n";
  calculate();

  std::cout << "End Calculation on Rank " << get_rank() << "\n";
  free_communication();

  MPI_Finalize();
}
