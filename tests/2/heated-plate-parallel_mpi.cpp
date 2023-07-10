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

#include "heated-plate-parallel_mpi.h"

// TODO: get an appropriate block size for best cache performance
#define MAX_BLK_SIZE 1200

#define MSG_Tag_1 1
#define MSG_Tag_2 2

void Matrix::calculate_num_local_rows(const int global_rows,
                                      const int global_columns, const int rank,
                                      const int numTasks) {
  int local_cols = 0, local_rows = 0;

  local_cols = global_columns;

  local_rows = global_rows / numTasks;

  if (rank < global_rows % numTasks) {
    // split the remainder equally among the first few processes
    ++local_rows;
  }

  this->rows = local_rows;
  this->columns = local_cols;
}

void Matrix::init_communication(const int rank, const int numTasks,
                                const int msg_tag) {

  int other = MPI_PROC_NULL;
  if (rank != 0) {
    other = rank - 1;
  }
  MPI_Send_init(data[1], this->columns, MPI_DOUBLE, other, msg_tag,
                MPI_COMM_WORLD, &comm_requests[0]);
  MPI_Recv_init(data[0], this->columns, MPI_DOUBLE, other, msg_tag,
                MPI_COMM_WORLD, &comm_requests[1]);

  other = MPI_PROC_NULL;
  if (rank != numTasks - 1) {
    other = rank + 1;
  }

  MPI_Send_init(data[this->rows], this->columns, MPI_DOUBLE, other, msg_tag,
                MPI_COMM_WORLD, &comm_requests[2]);
  MPI_Recv_init(data[this->rows + 1], this->columns, MPI_DOUBLE, other, msg_tag,
                MPI_COMM_WORLD, &comm_requests[3]);
}

void Matrix::init_matrix(const int rank, const int numTasks,
                         const double inner_value) {
  int i;

  //
  //  Initialize the interior solution to the mean value.
  //
  // the boundaries will be overwritten afterwards
  for (i = 0; i < rows + 1 + 1; i++) {
    for (int j = 0; j < columns + 1 + 1; j++) {
      data[i][j] = inner_value;
    }
  }

  //
  //  Set the boundary values, which don't change.
  //
  for (i = 0; i < rows + 1; i++) {
    data[i][0] = 100.0;
    data[i][columns + 1] = 100.0;
  }

  // has global lower boundary
  if (rank == numTasks - 1) {
    for (i = 0; i < columns + 1 + 1; i++) {
      data[rows + 1][i] = 100.0;
    }
  }

  // has global upper boundary
  if (rank == 0) {
    for (i = 0; i < columns + 1 + 1; i++) {
      data[0][i] = 0.0;
    }
  }
}

double run_iteration(double **Matrix_In, double **Matrix_Out, const int rows,
                     const int columns, const int size_of_block) {

  double diff = 0;
  const int number_of_blocks = (columns % size_of_block) == 0
                                   ? columns / size_of_block
                                   : 1 + (columns / size_of_block);

#pragma omp parallel for reduction(max : diff)
  for (int blk = 0; blk < number_of_blocks; ++blk) {
    const int block_begin = 1 + blk * size_of_block;
    const int block_end = std::min(1 + ((blk + 1) * size_of_block), columns);
    // #pragma omp simd collapse(2) nontemporal(Matrix_Out) reduction(max :
    // diff)
    for (int i = 1; i < rows + 1; i++) {
      // tell the compiler that we want to prefetch the next necessary line to
      // the cache
      //__builtin_prefetch(&Matrix_In[i + 1][1 + blk * size_of_block]);
      for (int j = block_begin; j < block_end; j++) {
        double new_val = (Matrix_In[i - 1][j] + Matrix_In[i + 1][j] +
                          Matrix_In[i][j - 1] + Matrix_In[i][j + 1]) /
                         4.0;
        // calculate difference to previous iteration
        diff = diff >= fabs(new_val - Matrix_In[i][j])
                   ? diff
                   : fabs(new_val - Matrix_In[i][j]);
        Matrix_Out[i][j] = new_val;
      }
    }
  }

  return diff;
}

//  iterate until the  new solution W differs from the old solution U
//  by no more than EPSILON.
// Matrix_Out is the input and the result, Matrix_in the buffer matrix
std::pair<int, double> calculate(const int rank, const double epsilon,
                                 Matrix &matrix_in, Matrix &matrix_out) {
  // int i, j;

  const int rows = matrix_in.rows;
  const int columns = matrix_in.columns;
  int iterations = 0;
  int iterations_print = 1;

  double diff;

  if (rank == 0) {
    std::cout << "\n";
    std::cout << " Iteration  Change\n";
    std::cout << "\n";
  }

  // get block size
#ifdef _OPENMP
  int threads = omp_get_max_threads();
#else
  int threads = 1;
#endif
  int size_of_block = columns / threads;
  // in case smaller blocks are needed for caching
  // choose a block size, so that all threads get an equal part
  while (size_of_block > MAX_BLK_SIZE) {
    size_of_block = size_of_block / 2;
  }

  diff = epsilon;
  // unroll the loop for two iterations, to eliminate the branch in the loop
  // TODO verify this claim in godbolt
#ifdef __clang__
#pragma clang loop unroll_count(2)
#elif __GNUG__
#pragma GCC unroll 2
#endif
  while (epsilon <= diff) {
    diff = 0.0;

    // just swap the pointer
    if (iterations % 2 == 0) // even
    {
      // begin receive from other rank
      matrix_out.begin_halo_receive();
      diff = run_iteration(matrix_out.data, matrix_in.data, rows, columns,
                           size_of_block);
      // in theory, it is possible to start sending the halo lines around right
      // after they are computed, but this will mess up the current
      // implementation with cache blocking
      matrix_out.begin_halo_send();

    } else { // odd
      matrix_in.begin_halo_receive();
      diff = run_iteration(matrix_in.data, matrix_out.data, rows, columns,
                           size_of_block);
      matrix_in.begin_halo_send();
    }

    MPI_Allreduce(MPI_IN_PLACE, &diff, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);

    // wait for communication to end
    if (iterations % 2 == 0) // even
    {
      matrix_out.end_halo_exchange();
    } else { // odd
      matrix_in.end_halo_exchange();
    }

    iterations++;
    if (iterations == iterations_print && rank == 0) {
      // print the progress of the calculation
      std::cout << "  " << std::setw(8) << iterations << "  " << diff << "\n";
      iterations_print = 2 * iterations_print;
    }
  } // end while

  // copy result into correct matrix if necessary
  if (iterations % 2 == 1) // odd
  {
    memcpy(matrix_out.data[0], matrix_in.data[0],
           sizeof(double) * (rows + 2) * (columns + 2));
  }

  return std::make_pair(iterations, diff);
}

//
//  Purpose:
//
//    MAIN is the main program for HEATED_PLATE_OPENMP.
//
//  Discussion:
//
//    This code solves the steady state heat equation on a rectangular region.
//
//    The sequential version of this program needs approximately
//    18/epsilon iterations to complete.
//
//
//    The physical region, and the boundary conditions, are suggested
//    by this diagram;
//
//                   W = 0
//             +------------------+
//             |                  |
//    W = 100  |                  | W = 100
//             |                  |
//             +------------------+
//                   W = 100
//
//    The region is covered with a grid of M by N nodes, and an M by N
//    array W is used to record the temperature.  The correspondence between
//    array indices and locations in the region is suggested by giving the
//    indices of the four corners:
//
//                  I = 0
//          [0][0]-------------[0][N-1]
//             |                  |
//      J = 0  |                  |  J = N-1
//             |                  |
//        [M-1][0]-----------[M-1][N-1]
//                  I = M-1
//
//    The steady state solution to the discrete heat equation satisfies the
//    following condition at an interior grid point:
//
//      W[Central] = (1/4) * ( W[North] + W[South] + W[East] + W[West] )
//
//    where "Central" is the index of the grid point, "North" is the index
//    of its immediate neighbor to the "north", and so on.
//
//    Given an approximate solution of the steady state heat equation, a
//    "better" solution is given by replacing each interior point by the
//    average of its 4 neighbors - in other words, by using the condition
//    as an ASSIGNMENT statement:
//
//      W[Central]  <=  (1/4) * ( W[North] + W[South] + W[East] + W[West] )
//
//    If this process is repeated often enough, the difference between
//    successive estimates of the solution will go to zero.
//
//    This program carries out such an iteration, using a tolerance specified by
//    the user.
//
//  Local parameters:
//
//    Local, double DIFF, the norm of the change in the solution from one
//    iteration to the next.
//
//    Local, double MEAN, the average of the boundary values, used to initialize
//    the values of the solution in the interior.
//
//    Local, double U[M][N], the solution at the previous iteration.
//
//    Local, double W[M][N], the solution computed at the latest iteration.
//
int main(int argc, char *argv[]) {
#define M 16
#define N 16

// #define M 10000
// #define N 10000
#define EPSILON 0.005

  int rank, numtasks;
  // Initialisiere Alle Prozesse
  MPI_Init(&argc, &argv);

  // Welchen rang habe ich?
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  // wie viele Tasks gibt es?
  MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

  //  Average the boundary values, to come up with a reasonable
  //  initial value for the interior.
  //
  double mean = N * 100 + N * 0 + M * 100 + M * 100 - 4 * 100;
  // -4 für die Ecken, die doppelt gezählt wurden
  mean = mean / (double)(2 * M + 2 * N - 4);
  if (rank == 0) {
    std::cout << "\n";
    std::cout << "  MEAN = " << mean << "\n";
  }
  {
    Matrix mIn(M, N, mean, MSG_Tag_1);
    Matrix mOut(M, N, mean, MSG_Tag_2);

    //
    //  iterate until the  new solution W differs from the old solution U
    //  by no more than EPSILON.
    //

    // barrier for time measurement: make sure all processes have finished
    // initialization
    MPI_Barrier(MPI_COMM_WORLD);
    double wtime = MPI_Wtime();
    double epsilon = EPSILON;
    // Structured binding declaration needs at least c++17
    auto [iterations, diff] = calculate(rank, epsilon, mIn, mOut);
    wtime = MPI_Wtime() - wtime;

    double max_time;

    // get the maximum time of all processes
    MPI_Reduce(&wtime, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
      std::cout << "\n";
      std::cout << "  " << std::setw(8) << iterations << "  " << diff << "\n";
      std::cout << "\n";
      std::cout << "  Error tolerance achieved.\n";
      std::cout << "  Wallclock time = " << max_time << "\n";
      std::cout << "\n";
      std::cout << "HEATED_PLATE_MPI:\n";
      std::cout << "  Normal end of execution.\n";
    }
  } // de-constructs teh matrix, so that all MPI ressources are freed

  MPI_Finalize();

  return 0;

#undef M
#undef N
}
