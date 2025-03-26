#ifndef MACH_OMP_FUNCS_H_
#define MACH_OMP_FUNCS_H_

#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"

struct omp_functions {
  llvm::Function *kmpc_fork_call = nullptr;

  llvm::Function *kmpc_global_thread_num = nullptr;
  llvm::Function *kmpc_push_num_threads = nullptr;
};

struct omp_functions *get_omp_functions(llvm::Module &M);

inline bool is_omp_function(llvm::Function *func) {
  auto omp_funcs = get_omp_functions(*func->getParent());
  return func->getName().starts_with("__kmpc_") ||
         func->getName().starts_with("omp_");

  return func == omp_funcs->kmpc_fork_call ||
         func == omp_funcs->kmpc_global_thread_num ||
         func == omp_funcs->kmpc_push_num_threads;
}

inline bool is_omp_fork_call(llvm::CallBase *call) {
  return (not call->isIndirectCall()) &&
         (call->getCalledFunction() ==
          get_omp_functions(*call->getModule())->kmpc_fork_call);
}

#endif /* MACH_OMP_FUNCS_H_ */
