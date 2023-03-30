#ifndef MPIOPT_PACK_H_
#define MPIOPT_PACK_H_

#include "request_type.h"
#include "settings.h"

#include <mpi.h>

LINKAGE_TYPE int opt_pack(MPIOPT_Request* request);
LINKAGE_TYPE int opt_unpack(MPIOPT_Request* request);

#endif /*MPIOPT_PACK_H*/