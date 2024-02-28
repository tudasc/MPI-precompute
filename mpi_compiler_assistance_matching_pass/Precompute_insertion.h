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
#ifndef MPI_ASSERTION_CHECKING_PRECOMPUTE_INSERTION_H
#define MPI_ASSERTION_CHECKING_PRECOMPUTE_INSERTION_H

#include "precalculation.h"
#include <llvm/IR/Module.h>

void insert_precomputation(
    llvm::Module &M, const PrecalculationAnalysis &precompute_analyis_result);

class PrecalculationFunctionCopy {
public:
  // Replacement Part
  explicit PrecalculationFunctionCopy(
      const std::shared_ptr<PrecalculationFunctionAnalysis> &analysis_result)
      : analysis_result(analysis_result), F_orig(analysis_result->func) {
    initialize_copy();
  }

  std::shared_ptr<PrecalculationFunctionAnalysis> analysis_result;
  llvm::Function *F_orig;
  llvm::Function *F_copy = nullptr;
  llvm::ValueToValueMapTy old_new_map;
  std::map<llvm::Value *, llvm::Value *> new_to_old_map;
  llvm::ClonedCodeInfo *cloned_code_info = nullptr; // currently we don't use it

private:
  void initialize_copy();
};

#endif // MPI_ASSERTION_CHECKING_PRECOMPUTE_INSERTION_H
