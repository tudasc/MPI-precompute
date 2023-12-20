
#ifndef INCLUDE_GUARD_DEVIRT_ANALYSIS_H
#define INCLUDE_GUARD_DEVIRT_ANALYSIS_H

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include <map>

class DevirtAnalysis {
public:
  DevirtAnalysis(llvm::Module &M);

  std::vector<llvm::Function *>
  get_possible_call_targets(llvm::CallBase *call) const {
    if (result_map.find(call) != result_map.end()) {
      return result_map.at(call);
    }
    else
      return {};
  }

private:
  std::map<llvm::CallBase *, std::vector<llvm::Function *>> result_map;
};

#endif // INCLUDE_GUARD_DEVIRT_ANALYSIS_H