#include <cassert>
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <string>
#include <vector>

double *DATA;

MPI_Request comm_requests[4];

std::vector<std::string> read_input_file(const std::string &fname) {

  std::ifstream file_in(fname);
  std::vector<std::string> lines;
  std::string line;

  while (std::getline(file_in, line))
    lines.push_back(line);
  assert(lines.size() > 0);
  return lines;
}

void init_communication(int rank, int size, int tag_to_use1, int tag_to_use2) {
  int other = MPI_PROC_NULL;
  if (rank != 0) {
    other = rank - 1;
  }
  MPI_Send_init(&DATA[25], 25, MPI_DOUBLE, other, tag_to_use1, MPI_COMM_WORLD,
                &comm_requests[0]);
  MPI_Recv_init(&DATA[0], 25, MPI_DOUBLE, other, tag_to_use2, MPI_COMM_WORLD,
                &comm_requests[1]);

  other = MPI_PROC_NULL;
  if (rank != size - 1) {
    other = rank + 1;
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
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  DATA = (double *)malloc(sizeof(double) * 100);
  for (int i = 0; i < 100; i++) {
    DATA[i] = 42.0;
  }

  auto file_content = read_input_file("config.cfg");
  int tag1 = std::stoi(file_content[0]);
  int tag2 = std::stoi(file_content[1]);

  init_communication(rank, size, tag1, tag2);

  calculate();

  free_communication();

  MPI_Finalize();
}
