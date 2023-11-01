#include "comm.h"
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

double *DATA;

void run_iteration() { return; }

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

int main(int argc, char *argv[]) {
  int rank, size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  DATA = malloc(sizeof(double) * 100);
  for (int i = 0; i < 100; i++) {
    DATA[i] = 42.0;
  }

  init_communication(rank, size);

  calculate();

  free_communication();

  MPI_Finalize();
}
