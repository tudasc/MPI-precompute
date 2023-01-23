#ifndef MPIOPT_SETTINGS_H_
#define MPIOPT_SETTINGS_H_

// config :
#define RDMA_SPIN_WAIT_THRESHOLD 32

#define STATISTIC_PRINTING
#define SUMMARY_STATISTIC_PRINTING
//#define BUFFER_CONTENT_CHECKING

//#define DISTORT_PROCESS_ORDER_ON_CROSSTALK

// linkage type of all internal functions
#ifndef LINKAGE_TYPE
#define LINKAGE_TYPE
//#define LINKAGE_TYPE static
#endif

#endif /* MPIOPT_SETTINGS_H_ */
