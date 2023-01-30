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

struct comm_info {
  // communication partners
  // may be MPI_PROC_NULL
  // if one calls an MPI routine with MPI_PROC_NULL as source or destination of
  // a message: this send/receive is a No-Op this makes the code more readable
  int up;
  int down;
  int left;
  int right;
};

// distributes the matrix across all processes
// returns (rows,columns) for local process and fills the comm_info struct
std::pair<int, int> distribute_matrix(struct comm_info &comm_partners, int rank,
                                      int numtasks, unsigned long N,
                                      unsigned long M);

class Matrix {
public:
  double **data;

  Matrix(int x, int y) { allocateMatrix(x, y); }

  ~Matrix() { freeMatrix(); }

  // may be useful for debugging
  void print_matrix(int N, int M) {
    for (int i = 0; i < N + 2; ++i) {
      std::cout << '\n';
      for (int j = 0; j < M + 2; ++j) {
        std::cout << std::fixed << std::setw(11) << std::setprecision(6)
                  << data[i][j];
      }
    }
    std::cout << '\n';
  }

  void init_matrix(struct comm_info comm_partners, int rows, int columns,
                   double inner_value);

private:
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