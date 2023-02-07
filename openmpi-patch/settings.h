#ifndef MPIOPT_SETTINGS_H_
#define MPIOPT_SETTINGS_H_

// config :
#define RDMA_SPIN_WAIT_THRESHOLD 32

// print what is going on
#define STATISTIC_PRINTING
// print a summery on the amount of crosstalk at the end
#define SUMMARY_STATISTIC_PRINTING
// check the contents of the buffers send around to detect race conditions
//#define BUFFER_CONTENT_CHECKING
// check that no matching conflicts exist to double check that the compiler
// analysis made no mistake
#define CHECK_FOR_MATCHING_CONFLICTS

// not mpi standard compliant, as this makes all init methods BLOCKing until all
// ranks win the comm initialize at least one request but makes dealing with
// multiple communicators easy to test things
#define REGISTER_COMMUNICATOR_ON_USE

// number of communicatros we can handle before running out of ressources
#define MAX_NUM_OF_COMMUNICATORS 64

// necessary, if one uses MPI_testsome:
// adds a small overhead to distinguish actifr from inactive requests
#define DISTINGUISH_ACTIVE_REQUESTS

//#define DISTORT_PROCESS_ORDER_ON_CROSSTALK

// linkage type of all internal functions
#ifndef LINKAGE_TYPE
#define LINKAGE_TYPE
//#define LINKAGE_TYPE static
#endif

#endif /* MPIOPT_SETTINGS_H_ */
