/*
Copyright 2020 Tim Jammer

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

#include "llvm/IR/Module.h"
#include <memory>

#ifndef MACH_TAINTED_VALUE_H
#define MACH_TAINTED_VALUE_H

// defined in ptr_info.h
class PtrUsageInfo;

enum TaintReason : int {
  OTHER = 0, // unspecified
  CONTROL_FLOW = 1 << 0,
  COMPUTE_TAG = 1 << 1,
  COMPUTE_DEST = 1 << 2,
};

struct TaintedValue {
  TaintedValue(llvm::Value *v) : v(v){};
  llvm::Value *v;
  int reason = OTHER;

  bool visited = false;

  // one can have multiple children and parents e.g. one call with several args
  // whose return value is used multiple times
  std::set<std::shared_ptr<TaintedValue>> children = {};
  std::set<std::shared_ptr<TaintedValue>> parents = {};
  // additional information for pointers
  std::shared_ptr<PtrUsageInfo> ptr_info = nullptr;

  bool is_pointer() { return v->getType()->isPointerTy(); };
};
#endif // MACH_TAINTED_VALUE_H