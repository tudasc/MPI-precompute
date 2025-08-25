#ifndef MACH_OMP_FUNCS_H_
#define MACH_OMP_FUNCS_H_

#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"

struct omp_functions {
  llvm::Function *kmpc_fork_call = nullptr;
  llvm::Function *kmpc_omp_task_alloc = nullptr;

  llvm::Function *kmpc_omp_task = nullptr;
  llvm::Function *kmpc_omp_task_with_deps = nullptr;

  llvm::Function *kmpc_global_thread_num = nullptr;
  llvm::Function *kmpc_push_num_threads = nullptr;
};

struct omp_functions *get_omp_functions(llvm::Module &M);

inline bool is_omp_function(const llvm::Function *func) {
  return func->getName().starts_with("__kmpc_") ||
         func->getName().starts_with("omp_");
}

inline bool is_pthread_function(const llvm::Function *func) {
  return func->getName().starts_with("pthread_");
}

inline bool is_thread_function(const llvm::Function *func) {
  return is_omp_function(func) || is_pthread_function(func);
}

inline bool is_pthread_create(llvm::CallBase *call) {
  return (not call->isIndirectCall()) &&
         (call->getCalledFunction() &&
          call->getCalledFunction()->getName() == "pthread_create");
}

inline bool is_omp_fork_call(llvm::CallBase *call) {
  return (not call->isIndirectCall()) &&
         (call->getCalledFunction() &&
          call->getCalledFunction() ==
              get_omp_functions(*call->getModule())->kmpc_fork_call);
}

inline bool is_thread_fork_call(llvm::CallBase *call) {
  return is_omp_fork_call(call) || is_pthread_create(call);
}

// get the call that actually schedules the task
inline std::vector<llvm::CallBase *>
get_task_scheduling_calls(llvm::CallBase *alloc_call) {
  assert(alloc_call->getCalledFunction() ==
         get_omp_functions(*alloc_call->getModule())->kmpc_omp_task_alloc);
  std::vector<llvm::CallBase *> sched_calls;
  for (auto *u : alloc_call->users()) {
    if (auto *call = llvm::dyn_cast<llvm::CallBase>(u)) {
      if (call->getCalledFunction() &&
          (call->getCalledFunction()->getName() == "__kmpc_omp_task" ||
           call->getCalledFunction()->getName() ==
               "__kmpc_omp_task_with_deps" ||
           call->getCalledFunction()->getName() == "__kmpc_taskloop" ||
           call->getCalledFunction()->getName() ==
               "__kmpc_omp_task_begin_if0")) {

        sched_calls.push_back(call);
        assert(call->getFunction() == alloc_call->getFunction());
      }
    }
  }
  if (sched_calls.empty()) {
    alloc_call->dump();
  }
  assert(!sched_calls.empty());
  return sched_calls;
}

#endif /* MACH_OMP_FUNCS_H_ */
