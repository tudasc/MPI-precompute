#ifndef MPIOPT_IFACE_H_
#define MPIOPT_IFACE_H_

#include <mpi.h>

// so that is easier to integrate it in mpi.h.in header
#ifndef OMPI_DECLSPEC
#define OMPI_DECLSPEC
#endif

// this comment serves as the marker for the build_patched_openmpi.sh script:
// do not change!
// START INTERFACE MPIOPT

OMPI_DECLSPEC int MPIOPT_Start(MPI_Request *request);
OMPI_DECLSPEC int MPIOPT_Startall(int count, MPI_Request array_of_requests[]);
OMPI_DECLSPEC int MPIOPT_Wait(MPI_Request *request, MPI_Status *status);
OMPI_DECLSPEC int MPIOPT_Waitall(int count, MPI_Request array_of_requests[],
                                 MPI_Status array_of_statuses[]);
OMPI_DECLSPEC int MPIOPT_Waitany(int count, MPI_Request array_of_requests[],
                                 int *index, MPI_Status *status);
OMPI_DECLSPEC int MPIOPT_Waitsome(int incount, MPI_Request array_of_requests[],
                                  int *outcount, int array_of_indices[],
                                  MPI_Status array_of_statuses[]);
OMPI_DECLSPEC int MPIOPT_Test(MPI_Request *request, int *flag,
                              MPI_Status *status);
OMPI_DECLSPEC int MPIOPT_Testany(int count, MPI_Request array_of_requests[],
                                 int *index, int *flag, MPI_Status *status);
OMPI_DECLSPEC int MPIOPT_Testall(int count, MPI_Request array_of_requests[],
                                 int *flag, MPI_Status array_of_statuses[]);
OMPI_DECLSPEC int MPIOPT_Testsome(int incount, MPI_Request array_of_requests[],
                                  int *outcount, int array_of_indices[],
                                  MPI_Status array_of_statuses[]);
OMPI_DECLSPEC int MPIOPT_Send_init(const void *buf, int count,
                                   MPI_Datatype datatype, int dest, int tag,
                                   MPI_Comm comm, MPI_Request *request);
OMPI_DECLSPEC int MPIOPT_Recv_init(void *buf, int count, MPI_Datatype datatype,
                                   int source, int tag, MPI_Comm comm,
                                   MPI_Request *request);
OMPI_DECLSPEC int MPIOPT_Send_init_x(const void *buf, int count,
                                     MPI_Datatype datatype, int dest, int tag,
                                     MPI_Comm comm, MPI_Request *request,
                                     MPI_Info info);
OMPI_DECLSPEC int MPIOPT_Recv_init_x(void *buf, int count,
                                     MPI_Datatype datatype, int source, int tag,
                                     MPI_Comm comm, MPI_Request *request,
                                     MPI_Info info);
OMPI_DECLSPEC int MPIOPT_Request_free(MPI_Request *request);

OMPI_DECLSPEC void MPIOPT_INIT();
OMPI_DECLSPEC void MPIOPT_FINALIZE();
OMPI_DECLSPEC void MPIOPT_Register_Communicator(MPI_Comm comm);

// END INTERFACE MPIOPT
#endif /* MPIOPT_IFACE_H_ */
