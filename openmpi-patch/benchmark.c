#define _GNU_SOURCE /* needed for some ompi internal headers*/

#include <inttypes.h>
#include <malloc.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "interface.h"

#include <execinfo.h>

#include <math.h>

#define WORK_BUFFER_SIZE 1000
#define NUM_ITERS 1000

#define MAX_BLOCKLENGTH 100
#define SEND_RATIO 0.4

#define tag_entry 42

// datatype create functions
void create_cont_data(MPI_Datatype* dtype, int size) {

}

void create_vector_data(MPI_Datatype* dtype, int size) {
    int block_len = MAX_BLOCKLENGTH;
    int stride = size / MAX_BLOCKLENGTH;
    if(size % MAX_BLOCKLENGTH) stride++;
    MPI_Type_vector();
}

void create_indexed_data(MPI_Datatype* dtype, int size) {

}

void create_struct_data(MPI_Datatype* dtype, int size) {

}

void create_combined_data(MPI_Datatype* dtype, int size) {

}

void use_one_sided_persistent(MPI_Datatype* dtype, int count, int size) {
    MPIOPT_INIT();

    int rank, numtasks;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

    char* buffer = malloc(size);
    double *work_buffer = calloc(WORK_BUFFER_SIZE, sizeof(double));
    work_buffer[WORK_BUFFER_SIZE - 1] = 0.6;

    MPI_Request req;

    if (rank == 1) {

        MPIOPT_Send_init(buffer, count, *dtype, 0, 42, MPI_COMM_WORLD, &req);

        for (int n = 0; n < NUM_ITERS; ++n) {
            for (int i = 0; i < size; ++i) {
                buffer[i] = 2 * (n + 1);
            }
            MPIOPT_Start(&req);
            dummy_workload(work_buffer);
            MPIOPT_Wait(&req, MPI_STATUS_IGNORE);
        }
    } else {

        MPIOPT_Recv_init(buffer, count, *dtype, 1, 42, MPI_COMM_WORLD, &req);

        for (int n = 0; n < NUM_ITERS; ++n) {
            for (int i = 0; i < size; ++i) {
                buffer[i] = (n + 1);
            }

            MPIOPT_Start(&req);
            dummy_workload(work_buffer);
            MPIOPT_Wait(&req, MPI_STATUS_IGNORE);
        }
    }

    MPIOPT_Request_free(&req);
    MPIOPT_FINALIZE();
}

void use_standard_comm(MPI_Datatype* dtype, int count, int size) {
    int rank, numtasks;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

    char* buffer = malloc(size);
    double *work_buffer = calloc(WORK_BUFFER_SIZE, sizeof(double));
    work_buffer[WORK_BUFFER_SIZE - 1] = 0.6;

    MPI_Request req;

    if (rank == 1) {

        for (int n = 0; n < NUM_ITERS; ++n) {
            for (int i = 0; i < size; ++i) {
                buffer[i] = 2 * (n + 1);
            }

            MPI_Isend(buffer, count, *dtype, 0, 42, MPI_COMM_WORLD, &req);
            dummy_workload(work_buffer);
            MPI_Wait(&req, MPI_STATUS_IGNORE);
        }
    } else {
        for (int n = 0; n < NUM_ITERS; ++n) {
            for (int i = 0; i < size; ++i) {
                buffer[i] = (n + 1);
            }

            MPI_Irecv(buffer, count, *dtype, 1, 42, MPI_COMM_WORLD, &req);
            dummy_workload(work_buffer);
            MPI_Wait(&req, MPI_STATUS_IGNORE);
        }
    }
    MPI_Request_free(&req);
}

void use_persistent_comm(MPI_Datatype* dtype, int count, int size) {
    int rank, numtasks;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

    char* buffer = malloc(size);
    double *work_buffer = calloc(WORK_BUFFER_SIZE, sizeof(double));
    work_buffer[WORK_BUFFER_SIZE - 1] = 0.6;

    MPI_Request req;
    if (rank == 1) {

        MPI_Send_init(buffer, count, *dtype, 0, 42, MPI_COMM_WORLD, &req);

        for (int n = 0; n < NUM_ITERS; ++n) {
            for (int i = 0; i < size; ++i) {
                buffer[i] = 2 * (n + 1);
            }

            MPI_Start(&req);
            dummy_workload(work_buffer);
            MPI_Wait(&req, MPI_STATUS_IGNORE);
        }
    } else {

        MPI_Recv_init(buffer, count, *dtype, 1, 42, MPI_COMM_WORLD, &req);
        for (int n = 0; n < NUM_ITERS; ++n) {
            for (int i = 0; i < size; ++i) {
                buffer[i] = (n + 1);
            }

            MPI_Start(&req);
            dummy_workload(work_buffer);
            MPI_Wait(&req, MPI_STATUS_IGNORE);
        }
    }

    MPI_Request_free(&req);
}


int main(int argc, char **argv) {
    struct timeval start_time;
    struct timeval stop_time;

    MPI_Init(&argc, &argv);
}