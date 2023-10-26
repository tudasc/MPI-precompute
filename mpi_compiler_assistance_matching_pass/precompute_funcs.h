
#ifndef MACH_PRECOMPUTE_FUNCS_H_
#define MACH_PRECOMPUTE_FUNCS_H_

#include "llvm/IR/Constant.h"
#include "llvm/IR/Module.h"
#include <cassert>
#include <llvm/IR/InstrTypes.h>

// holds the llvm::Function* for the functions from the precompute library
class PrecomputeFunctions {

public:
  // singelton-like
  static PrecomputeFunctions *get_instance() {
    assert(instance != nullptr);
    return instance;
  }

  static PrecomputeFunctions *create_instance(llvm::Module &M) {
    if (instance == nullptr)
      instance = new PrecomputeFunctions(M);
    return instance;
  }

  static void delete_instance() {
    if (instance != nullptr)
      delete instance;
    instance = nullptr;
  }

  // singelton pattern: singeltons are NOT copy-able or assignable
  PrecomputeFunctions(PrecomputeFunctions &other) = delete;

  void operator=(const PrecomputeFunctions &) = delete;

  bool is_call_to_precompute(llvm::Function *func) {
    return func == init_precompute_lib || func == finish_precomputation ||
           func == register_precomputed_value || func == allocate_memory;
  }

  bool is_call_to_precompute(llvm::CallBase *call) {
    if (call->isIndirectCall()) {
      return false;
    }
    return is_call_to_precompute(call->getCalledFunction());
  }

private:
  static PrecomputeFunctions *instance;

  PrecomputeFunctions(llvm::Module &M);

  ~PrecomputeFunctions(){};

public:
  llvm::Function *init_precompute_lib;
  llvm::Function *finish_precomputation;
  llvm::Function *register_precomputed_value;
  llvm::Function *allocate_memory;
};

#endif /* MACH_PRECOMPUTE_FUNCS_H_ */
