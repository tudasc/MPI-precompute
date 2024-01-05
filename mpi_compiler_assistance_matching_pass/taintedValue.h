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
  OTHER = 0,         // unspecified
  ANALYSIS = 1 << 0, // is analyzed if we need to include it
  COMPUTE_TAG = 1 << 1,
  COMPUTE_DEST = 1 << 2,
  CONTROL_FLOW = 1 << 3,

  // Bitmask to select only the genreal reasons to propergate tot the specific
  // reason
  REASONS_TO_PROPERGATE = ANALYSIS | COMPUTE_TAG | COMPUTE_DEST | CONTROL_FLOW,

  // need the return value of a call not only its presence:
  // implies that the control flow need to pass to the callee to calculate
  // return value
  CONTROL_FLOW_RETURN_VALUE_NEEDED = CONTROL_FLOW | 1 << 4,
  // need the control flow to pass this call to reach callee
  CONTROL_FLOW_CALLEE_NEEDED = CONTROL_FLOW | 1 << 5,
  // need the control flow to call this call to check for exception
  CONTROL_FLOW_EXCEPTION_NEEDED = CONTROL_FLOW | 1 << 6,
  // for invoke: this invoke can be replaced with an unconditional br to normal
  // dest as exception handling code is not relevant for precompute nor is the
  // return value
  CONTROL_FLOW_ONLY_PRESENCE_NEEDED = CONTROL_FLOW | 1 << 7,
};

struct TaintedValue {
  TaintedValue(llvm::Value *v) : v(v){};
  llvm::Value *v = nullptr;

private:
  int _reason = OTHER;
  bool _include_in_precompute = false;

public:
  int getReason() const { return _reason; }
  inline void addReason(int reason) {
    _reason = _reason | reason;

    if (_reason & (CONTROL_FLOW_CALLEE_NEEDED xor CONTROL_FLOW)) {
      assert(llvm::isa<llvm::CallBase>(v));
    }
    if (_reason & (CONTROL_FLOW_RETURN_VALUE_NEEDED xor CONTROL_FLOW)) {
      assert(llvm::isa<llvm::CallBase>(v));
    }
    if (_reason & (CONTROL_FLOW_EXCEPTION_NEEDED xor CONTROL_FLOW)) {
      assert(llvm::isa<llvm::InvokeInst>(v));
    }
    if (_reason & (CONTROL_FLOW_ONLY_PRESENCE_NEEDED xor CONTROL_FLOW)) {
      assert(llvm::isa<llvm::InvokeInst>(v));
    }
  }
  inline bool has_specific_reason() const {
    if (not llvm::isa<llvm::CallBase>(v)) {
      assert(not(_reason & ~REASONS_TO_PROPERGATE));
      return true;
    } else {
      return _reason & ~REASONS_TO_PROPERGATE;
    }
  };

  bool isIncludeInPrecompute() const { return _include_in_precompute; }
  void setIncludeInPrecompute() {
    if (not _include_in_precompute) {
      // assert(_reason & ANALYSIS);
      _reason = _reason & (~ANALYSIS); // remove the reason ANALYSIS
      _include_in_precompute = true;
      // TODO do I rly need to re-visit it?
      visited = false;
    }
  }

public:
  bool visited = false;

  // one can have multiple children and parents e.g. one call with several args
  // whose return value is used multiple times
  std::set<std::shared_ptr<TaintedValue>> needs = {};
  std::set<std::shared_ptr<TaintedValue>> needed_for = {};
  // additional information for pointers
  std::shared_ptr<PtrUsageInfo> ptr_info = nullptr;

  bool is_pointer() { return v->getType()->isPointerTy(); };
};
#endif // MACH_TAINTED_VALUE_H