//  Licensing:
//
//    This code is distributed under the GNU LGPL license.
//
//  Modified:
//
//    24 October 2019
//
//  Author:
//
//    Original C version by Michael Quinn.
//    C++ version by John Burkardt,
//    slightly adapted by Tim Jammer and Yannic Fischler.
//
//  Reference:
//
//    Michael Quinn,
//    Parallel Programming in C with MPI and OpenMP,
//    McGraw-Hill, 2004,
//    ISBN13: 978-0071232654,
//    LC: QA76.73.C15.Q55.

#include <assert.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include <omp.h>

#include <mpi.h>

class Matrix {
public:
  double **data;
  MPI_Request comm_requests[4];
  int rows, columns;

  Matrix(const int global_rows, const int global_columns,
         const double inner_value) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    calculate_num_local_rows(global_rows, global_columns, rank, size);
    // allocate additional space for the halo lines
    allocateMatrix(this->rows + 2, this->columns + 2);
    init_communication(rank, size);
    init_matrix(rank, size, inner_value);
  }

  Matrix(const Matrix &other) {
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    this->rows = other.rows;
    this->columns = other.columns;
    // allocate additional space for the halo lines
    allocateMatrix(this->rows + 2, this->columns + 2);
    init_communication(rank, size);
    memcpy(this->data[0], other.data[0],
           sizeof(double) * (other.rows + 2) * (other.columns + 2));
  }

  ~Matrix() {
    for (int i = 0; i < 4; ++i) {
      if (comm_requests[i] != MPI_REQUEST_NULL) {
        MPI_Request_free(&comm_requests[i]);
      }
    }
    freeMatrix();
  }

  // may be useful for debugging
  void print_matrix(const int N, const int M) {
    for (int i = 0; i < N + 2; ++i) {
      std::cout << '\n';
      for (int j = 0; j < M; ++j) {
        std::cout << std::fixed << std::setw(11) << std::setprecision(6)
                  << data[i][j];
      }
    }
    std::cout << '\n';
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
  static int tag_to_use;

  void init_matrix(const int rank, const int numTasks,
                   const double inner_value);

  void calculate_num_local_rows(const int global_rows, const int global_columns,
                                const int rank, const int numTasks);

  void init_communication(const int rank, const int numTasks);
  /* ************************************************************************ */
  /* helper function: freeMatrix: frees memory of the matrix                  */
  /* ************************************************************************ */
  void freeMatrix() {
    free(data[0]);
    data[0] = nullptr;
    free(data);
    data = nullptr;
  }

  /* ************************************************************************ */
  /* helper function: allocateMatrix: allocates memory for a matrix           */
  /* ************************************************************************ */
  void allocateMatrix(int x, int y) {
    int i;

    double *data_layer = (double *)malloc(x * y * sizeof(double));
    double **resultmatrix = (double **)malloc(x * sizeof(double *));

    if (data_layer == NULL || resultmatrix == NULL) {
      printf("ERROR ALLOCATING MEMORY\n");
      exit(1);
    }

    for (i = 0; i < x; i++) {
      resultmatrix[i] = data_layer + i * y;
    }

    data = resultmatrix;
  }
};

//  iterate until the  new solution W differs from the old solution U
//  by no more than EPSILON.
// Matrix_Out is the input and the result, Matrix_in the buffer matrix
std::pair<int, double> calculate(int rank, double epsilon, int rows,
                                 int columns, Matrix &Matrix_In,
                                 Matrix &Matrix_Out,
                                 struct comm_info comm_partners);
