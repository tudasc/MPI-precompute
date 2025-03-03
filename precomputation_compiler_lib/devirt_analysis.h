/*
Copyright 2023 Tim Jammer

Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/
#ifndef INCLUDE_GUARD_DEVIRT_ANALYSIS_H
#define INCLUDE_GUARD_DEVIRT_ANALYSIS_H

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include <map>

class DevirtAnalysis {
protected:
  DevirtAnalysis(llvm::Module &M);
  static DevirtAnalysis *instance;

public:
  static std::vector<llvm::Function *>
  get_possible_call_targets(llvm::CallBase *call) {
    if (instance == nullptr) {
      // populate the result map
      instance = new DevirtAnalysis(*call->getModule());
    }

    if (instance->result_map.find(call) != instance->result_map.end()) {
      return instance->result_map.at(call);
    } else
      return {};
  }

  // singleton Pattern
  // (one should never acquire the instance of this class anyway)
  DevirtAnalysis(DevirtAnalysis &other) = delete;
  void operator=(const DevirtAnalysis &) = delete;

private:
  std::map<llvm::CallBase *, std::vector<llvm::Function *>> result_map;
};

#endif // INCLUDE_GUARD_DEVIRT_ANALYSIS_H