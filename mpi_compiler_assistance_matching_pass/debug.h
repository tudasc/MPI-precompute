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
#ifndef MACH_DEBUG_H
#define MACH_DEBUG_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"
#include <boost/stacktrace.hpp>
#include <llvm/IR/Constants.h>

#if DEBUG_MACH_PASS == 1
#define Debug(x) x
#else
#define Debug(x)
#endif

void add_debug_printfs_to_precalculation(llvm::Function *func);

inline int get_num_undefs(const llvm::Function &F) {
  int num_undef = 0;
  for (auto &BB : F) {
    for (auto &I : BB) {
      // Check if the instruction has any undef operands.
      for (auto &U : I.operands()) {
        if (U && llvm::isa<llvm::UndefValue>(U)) {
          num_undef++;
        }
      }
    }
  }
  return num_undef;
}

inline int get_num_undefs(const llvm::Module &M) {
  int num_undef = 0;
  for (auto &F : M) {
    num_undef += get_num_undefs(F);
  }
  return num_undef;
}

#endif
