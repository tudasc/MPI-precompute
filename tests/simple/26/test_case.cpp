#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <mpi.h>
#include <string>
#include <vector>

double *DATA;

MPI_Request comm_requests[4];

class Arguments_reader {
private:
  std::vector<std::string> m_argv; // copy of argv
  std::map<std::string, std::string> m_require_args;
  std::map<std::string, std::string> m_facultative_args;
  std::map<std::string, std::string> m_args;
  std::string m_program_name;

public:
  Arguments_reader(int argc, char **argv) : m_argv(argc) {
    assert(argc > 0);

    this->m_program_name = argv[0];

    for (unsigned short i = 0; i < argc; ++i)
      this->m_argv[i] = argv[i];
  }
  virtual ~Arguments_reader(){};
  bool parse_arguments(std::map<std::string, std::string> requireArgs,
                       std::map<std::string, std::string> facultativeArgs);
  bool exist_argument(std::string tag) {
    return (this->m_args.find(tag) != this->m_args.end());
  }
  std::string get_argument(std::string tag) { return this->m_args[tag]; }
  // void print_usage();

private:
  bool sub_parse_arguments(std::map<std::string, std::string> args,
                           unsigned short posArg);
  void clear_arguments() {
    this->m_require_args.clear();
    this->m_facultative_args.clear();
    this->m_args.clear();
  }
};

bool Arguments_reader::parse_arguments(
    std::map<std::string, std::string> requireArgs,
    std::map<std::string, std::string> facultativeArgs) {
  // assert(requireArgs.size() > 0); // useless, it is possible to have no
  // require arguments
  unsigned short int nReqArg = 0;

  this->clear_arguments();

  this->m_require_args = requireArgs;
  this->m_facultative_args = facultativeArgs;

  for (unsigned short i = 0; i < this->m_argv.size(); ++i) {
    if (this->sub_parse_arguments(this->m_require_args, i))
      nReqArg++;
    this->sub_parse_arguments(this->m_facultative_args, i);
  }

  return nReqArg >= requireArgs.size();
}

bool Arguments_reader::sub_parse_arguments(
    std::map<std::string, std::string> args, unsigned short posArg) {
  assert(posArg < this->m_argv.size());

  bool isFound = false;

  for (auto it = args.begin(); it != args.end(); ++it) {
    std::string curArg = "-" + it->first;
    if (curArg == this->m_argv[posArg]) {
      if (it->second != "") {
        if (posArg != (this->m_argv.size() - 1)) {
          this->m_args[it->first] = this->m_argv[posArg + 1];
          isFound = true;
        }
      } else {
        this->m_args[it->first] = "";
        isFound = true;
      }
    }
  }

  return isFound;
}

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

  std::map<std::string, std::string> facul_args;
  facul_args["t1"] = "msg_tag1";
  facul_args["t2"] = "msg_tag2";

  Arguments_reader arg_reader(argc, argv);
  arg_reader.parse_arguments({}, facul_args);

  int tag1 = 42;
  int tag2 = 42 + 42;

  if (arg_reader.exist_argument("t1")) {
    tag1 = std::stoi(arg_reader.get_argument("t1"));
  }
  if (arg_reader.exist_argument("t2")) {
    tag2 = std::stoi(arg_reader.get_argument("t2"));
  }

  init_communication(tag1, tag2);

  std::cout << "Start Calculation on Rank " << get_rank() << "\n";
  calculate();

  std::cout << "End Calculation on Rank " << get_rank() << "\n";
  free_communication();

  MPI_Finalize();
}
