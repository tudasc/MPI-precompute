// TOOD Rename File with more appropriate name
#ifndef LOW_LEVEL_H_
#define LOW_LEVEL_H_

#include <mpi.h>

int MPIOPT_Start(MPI_Request *request);
int MPIOPT_Wait(MPI_Request *request, MPI_Status *status);
int MPIOPT_Waitall(int count, MPI_Request array_of_requests[],
                MPI_Status array_of_statuses[]);
int MPIOPT_Waitany(int count, MPI_Request array_of_requests[], int *index,MPI_Status *status);
int MPIOPT_Waitsome(int incount, MPI_Request array_of_requests[],
                 int *outcount, int array_of_indices[],
                 MPI_Status array_of_statuses[]);
int MPIOPT_Test(MPI_Request *request, int *flag, MPI_Status *status);
int MPIOPT_Testany(int count, MPI_Request array_of_requests[], int *index,
                int *flag, MPI_Status *status);
int MPIOPT_Testall(int count, MPI_Request array_of_requests[], int *flag,
                MPI_Status array_of_statuses[]);
int MPIOPT_Testsome(int incount, MPI_Request array_of_requests[],
                 int *outcount, int array_of_indices[],
                 MPI_Status array_of_statuses[]);
int MPIOPT_Send_init(const void *buf, int count, MPI_Datatype datatype,
                     int dest, int tag, MPI_Comm comm, MPI_Request *request);
int MPIOPT_Recv_init(void *buf, int count, MPI_Datatype datatype, int source,
                     int tag, MPI_Comm comm, MPI_Request *request);
int MPIOPT_Request_free(MPI_Request *request);

void MPIOPT_INIT();
void MPIOPT_FINALIZE();

#endif /* LOW_LEVEL_H_ */
