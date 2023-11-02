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

#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"
#include <memory>

#ifndef MACH_TAINTED_VALUE_H
#define MACH_TAINTED_VALUE_H

// defined in ptr_info.h
class PtrUsageInfo;

enum TaintReason : int {
  OTHER = 0, // unspecified
  COMPUTE_TAG = 1 << 0,
  COMPUTE_DEST = 1 << 1,

  CONTROL_FLOW = 1 << 2,
  // need the return value of a call not only its presence:
  // implies that the control flow need to pass to the callee to calculate
  // return value
  CONTROL_FLOW_RETURN_VALUE_NEEDED = CONTROL_FLOW | 1 << 3,
  // need the control flow to pass this call to reach callee
  CONTROL_FLOW_CALLEE_NEEDED = CONTROL_FLOW | 1 << 4,
  // need the control flow to call this call to check for exception
  CONTROL_FLOW_EXCEPTION_NEEDED = CONTROL_FLOW | 1 << 5,
  // for invoke: this invoke can be replaced with an unconditional br to normal
  // dest as exception handling code is not relevant for precompute nor is the
  // return value
  CONTROL_FLOW_ONLY_PRESENCE_NEEDED = CONTROL_FLOW | 1 << 6,
};

struct TaintedValue {
  TaintedValue(llvm::Value *v) : v(v){};
  llvm::Value *v = nullptr;

private:
  int _reason = OTHER;

public:
  int getReason() const { return _reason; }
  void addReason(int reason) {
    _reason = _reason | reason;

    if (_reason & CONTROL_FLOW_CALLEE_NEEDED) {
      assert(llvm::isa<llvm::CallBase>(v));
    }
    if (_reason & CONTROL_FLOW_RETURN_VALUE_NEEDED) {
      assert(llvm::isa<llvm::CallBase>(v));
    }
    if (_reason & CONTROL_FLOW_EXCEPTION_NEEDED) {
      assert(llvm::isa<llvm::InvokeInst>(v));
    }
    if (_reason & CONTROL_FLOW_ONLY_PRESENCE_NEEDED) {
      assert(llvm::isa<llvm::InvokeInst>(v));
    }
  }

public:
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