#ifndef CATO_MICROTASK_H
#define CATO_MICROTASK_H

#include <map>
#include <memory>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>

/**
 * Struct with pointers to OpenMP Runtime Library calls for parallel for loops
 **/
struct ParallelForData {
  llvm::CallInst *init;
  llvm::CallInst *fini;
};

/**
 * Struct with pointer to OpenMP Runtime Library calls for reduction pragmas
 **/
struct ReductionData {
  llvm::CallInst *reduce;
  llvm::CallInst *end_reduce;
};

/**
 * This class represent an Openmp Parallel Region (omp.outlined) which is
 *created for each OpenMP parallel section.
 **/
class ParallelRegion {
private:
  // The __kmpc_fork_call s instruction in the original code, which calls the
  // OpenMP microtask
  std::vector<llvm::CallBase *> _fork_calls;

  // The outlined function itself (omp.outlined created by the compiler for
  // OpenMP parallel sections).
  llvm::Function *_function;

  // map to match values in serial and parallel region
  std::map<llvm::Value *, std::vector<llvm::Value *>> _to_serial_map;
  std::map<llvm::Value *, llvm::Value *> _to_parallel_map;

  // Parallel for inside the microtask
  ParallelForData _parallel_for;

  // Reduction inside the microtask
  ReductionData _reduction;

public:
  /**
   * Constructor expects the ompoutlined function
   **/
  ParallelRegion(llvm::Function *ompoutlined);

  ~ParallelRegion();

  std::vector<llvm::CallBase *> get_fork_calls();

  llvm::Function *get_function();

  ParallelForData *get_parallel_for();

  ReductionData *get_reduction();

  std::vector<std::pair<llvm::Value *, llvm::Value *>> &get_shared_variables();

  // gets the value that corresponds to the given value from serial region
  llvm::Argument *get_value_in_parallel(llvm::Value *val);
  // get the value that corresponds to the given value in parallel region
  std::vector<llvm::Value *> get_value_in_serial(llvm::Value *val);

  // get end block of loop
  // this means the omp.dispatch.cond.omp.dispatch.end_crit_edge
  // not the omp.dispatch.end block with the call to for_static_fini function
  llvm::BasicBlock *find_loop_end_block();
};

#endif
