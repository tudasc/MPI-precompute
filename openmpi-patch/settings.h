#ifndef MPIOPT_SETTINGS_H_
#define MPIOPT_SETTINGS_H_

// config :
#define RDMA_SPIN_WAIT_THRESHOLD 32

// used for the tests to check if the version without matching is actually used
// (or if the compile time analysis failed for some reason)
#define PRINT_SUCCESSFUL_HANDSHAKE

#define DUMP_DEBUG_TRACE_EVERY_TIME
// print a summary on the amount of crosstalk at the end
#define SUMMARY_STATISTIC_PRINTING
// check the contents of the buffers send around to detect race conditions
// #define BUFFER_CONTENT_CHECKING
// check that no matching conflicts exist to double check that the compiler
// analysis made no mistake
#define CHECK_FOR_MATCHING_CONFLICTS

// not mpi standard compliant, as this makes all init methods BLOCKing until all
// ranks win the comm initialize at least one request but makes dealing with
// multiple communicators easy to test things
#define REGISTER_COMMUNICATOR_ON_USE

// number of communicatros we can handle before running out of ressources
#define MAX_NUM_OF_COMMUNICATORS 64

// print warning that deadlocks/left over resources are possible when freeing
// unused requests
// #define WARN_ON_REQUEST_FREE

// necessary, if one uses MPI_testsome:
// adds a small overhead to distinguish active from inactive requests
// #define DISTINGUISH_ACTIVE_REQUESTS

// #define DISTORT_PROCESS_ORDER_ON_CROSSTALK

// #define USE_FALLBACK_UNTIL_THRESHOLD
// #define FALLBACK_THRESHOLD 1000

#define WAIT_ON_STARTALL_TO_PREVENT_CROSSTALK
#define WAIT_ON_STARTALL_WAIT_TIME 0
// this should be based on latency
// TODO Implementation should automatically adjust this time in necesssary

// linkage type of all internal functions
#ifndef LINKAGE_TYPE
#define LINKAGE_TYPE
// #define LINKAGE_TYPE static
#endif

#define DEFAULT_THRESHOLD 20 // default threshold for mixed sending in bytes
// strategies to handle non contiguous datatypes
#define NC_PACKING 0
#define NC_DIRECT_SEND 1
#define NC_OPT_PACKING 2
#define NC_MIXED 3

#endif /* MPIOPT_SETTINGS_H_ */
