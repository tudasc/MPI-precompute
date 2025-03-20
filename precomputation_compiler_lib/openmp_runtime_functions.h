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

#endif /* MACH_OMP_FUNCS_H_ */
