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

#include <utility>

#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/Cloning.h"

#ifndef MACH_PRECALCULATIONS_H_
#define MACH_PRECALCULATIONS_H_

class FunctionToPrecalculate {
public:
  FunctionToPrecalculate(llvm::Function *F, std::set<unsigned int> args_to_use)
      : args_to_use(std::move(args_to_use)), F_orig(F){};
  void add_relevant_args(const std::set<unsigned int> &new_args_to_use) {
    std::copy(new_args_to_use.begin(), new_args_to_use.end(),
              std::inserter(args_to_use, args_to_use.begin()));
  }

private:
  std::set<unsigned int> args_to_use;
  llvm::Function *F_orig;
};

class Precalculations {
public:
  Precalculations(llvm::Module &M, llvm::Function *entry_point)
      : M(M), entry_point(entry_point){};

  void add_precalculations(const std::vector<llvm::CallBase *> &to_precompute);

private:
  llvm::Module &M;
  llvm::Function *entry_point;

  std::vector<llvm::CallBase *> to_replace_with_envelope_register;
  std::set<std::unique_ptr<FunctionToPrecalculate>> functions_to_include;
  std::set<llvm::Value *> tainted_values;
  std::set<llvm::Value *> visited_values;

  void find_all_tainted_vals();
  void visit_val(llvm::Value *v);
};

#endif // MACH_PRECALCULATIONS_H_