#ifndef MPIOPT_SETTINGS_H_
#define MPIOPT_SETTINGS_H_

// config :
#define RDMA_SPIN_WAIT_THRESHOLD 32

#define STATISTIC_PRINTING
#define SUMMARY_STATISTIC_PRINTING
//#define BUFFER_CONTENT_CHECKING

// not mpi standard compliant, as this makes all init methods BLOCKing until all
// ranks win the comm initialize at least one request but makes dealing with
// multiple communicators easy to test things
#define REGISTER_COMMUNICATOR_ON_USE

#define MAX_NUM_OF_COMMUNICATORS 64

// necessary, if one uses MPI_testsome:
#define DISTINGUISH_ACTIVE_REQUESTS

//#define DISTORT_PROCESS_ORDER_ON_CROSSTALK

// linkage type of all internal functions
#ifndef LINKAGE_TYPE
#define LINKAGE_TYPE
//#define LINKAGE_TYPE static
#endif

#endif /* MPIOPT_SETTINGS_H_ */
