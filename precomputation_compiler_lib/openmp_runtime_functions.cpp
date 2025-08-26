#include "openmp_runtime_functions.h"
#include <assert.h>

#include "llvm/IR/InstrTypes.h"
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

struct omp_functions *omp_func = nullptr;

struct omp_functions *get_omp_functions(llvm::Module &M) {
  if (omp_func) {
    return omp_func;
  }

  omp_func = new omp_functions();

  for (auto it = M.begin(); it != M.end(); ++it) {
    Function *f = &*it;
    if (f->getName() == "__kmpc_fork_call") {
      omp_func->kmpc_fork_call = f;
    } else if (f->getName() == "__kmpc_global_thread_num") {
      omp_func->kmpc_global_thread_num = f;
    } else if (f->getName() == "__kmpc_push_num_threads") {
      omp_func->kmpc_push_num_threads = f;
    } else if (f->getName() == "__kmpc_omp_task_alloc") {
      omp_func->kmpc_omp_task_alloc = f;
    } else if (f->getName() == "__kmpc_omp_task") {
      omp_func->kmpc_omp_task = f;
    } else if (f->getName() == "__kmpc_omp_task_with_deps") {
      omp_func->kmpc_omp_task_with_deps = f;
    }
  }

  return omp_func;
}
