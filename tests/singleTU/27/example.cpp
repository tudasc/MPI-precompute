#include <iostream>
#include <memory>
#include <mpi.h>

double *DATA;

class CommManager {
public:
  virtual ~CommManager(){};
  virtual void begin_halo_receive() = 0;

  virtual void begin_halo_send() = 0;

  virtual void end_halo_exchange() = 0;
};

class CommManagerStandard : public CommManager {
public:
  CommManagerStandard(int tag_to_use) : tag_to_use(tag_to_use) {
    std::cout << "Standard Comm\n";
  }
  ~CommManagerStandard() {}
  void begin_halo_receive() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int other = MPI_PROC_NULL;
    if (rank != 0) {
      other = rank - 1;
    }
    MPI_Irecv(&DATA[0], 25, MPI_DOUBLE, other, tag_to_use, MPI_COMM_WORLD,
              &comm_requests[1]);

    other = MPI_PROC_NULL;
    if (rank != size - 1) {
      other = rank + 1;
    }
    MPI_Irecv(&DATA[50], 25, MPI_DOUBLE, other, tag_to_use, MPI_COMM_WORLD,
              &comm_requests[3]);
  }

  void begin_halo_send() {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int other = MPI_PROC_NULL;
    if (rank != 0) {
      other = rank - 1;
    }
    MPI_Isend(&DATA[25], 25, MPI_DOUBLE, other, tag_to_use, MPI_COMM_WORLD,
              &comm_requests[0]);

    other = MPI_PROC_NULL;
    if (rank != size - 1) {
      other = rank + 1;
    }
    MPI_Isend(&DATA[75], 25, MPI_DOUBLE, other, tag_to_use, MPI_COMM_WORLD,
              &comm_requests[2]);
  }

  void end_halo_exchange() {
    // could also use MPI_waitall
    for (int i = 0; i < 4; ++i) {
      MPI_Wait(&comm_requests[i], MPI_STATUS_IGNORE);
    }
  }

private:
  int tag_to_use;
  MPI_Request comm_requests[4];
};

class CommManagerPersistent : public CommManager {
public:
  CommManagerPersistent(int tag_to_use) {
    std::cout << "Persistent Comm\n";
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

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
  }
  ~CommManagerPersistent() {
    MPI_Request_free(&comm_requests[0]);
    MPI_Request_free(&comm_requests[1]);
    MPI_Request_free(&comm_requests[2]);
    MPI_Request_free(&comm_requests[3]);
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

private:
  MPI_Request comm_requests[4];
};

std::shared_ptr<CommManager> mgmr;

void run_iteration() { return; }

void calculate() {
  char running = 1;
  int iter = 0;
  while (running) {
    mgmr->begin_halo_receive();

    run_iteration();

    mgmr->begin_halo_send();

    mgmr->end_halo_exchange();

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

  DATA = (double *)malloc(sizeof(double) * 100);
  for (int i = 0; i < 100; i++) {
    DATA[i] = 42.0;
  }

  if (argc == 1) {
    mgmr = std::make_shared<CommManagerPersistent>(42);

  } else {
    mgmr = std::make_shared<CommManagerStandard>(42);
  }

  calculate();

  mgmr = nullptr; // trigger destructor

  MPI_Finalize();
}
