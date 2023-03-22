#include "wait.h"
#include "globals.h"
#include "handshake.h"
#include "settings.h"

#include "test.h"

#include "debug.h"
#include "mpi-internals.h"

#include <stdlib.h>
#include <unistd.h>

LINKAGE_TYPE void wait_for_completion_blocking(void *request) {
  assert(request != NULL);
  ucs_status_t status;
  do {
    ucp_worker_progress(mca_osc_ucx_component.ucp_worker);
    status = ucp_request_check_status(request);
  } while (status == UCS_INPROGRESS);
  ucp_request_free(request);
}

// operation_number*2= op has not started on remote
// operation_number*2 +1= op has started on remote, we should initiate
// data-transfer operation_number*2 + 2= op has finished on remote
