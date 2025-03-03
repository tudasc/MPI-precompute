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

#include <map>
#include "precalculation_impl.h"
#include "precalculation_function_analysis.h"
#include "llvm/Transforms/Utils/Cloning.h"

class PrecalculationAnalysisImpl; // such that include order doesn't matter
class PrecalculationFunctionAnalysis;


#include <llvm/IR/Module.h>

class PrecalculationFunctionCopy;

// doesn't need to be a class, but that way it is easier to friend the
// PrecalculationAnalysisImpl so that we can reference its internal status
class PrecomputeInsertion {
public:
  PrecomputeInsertion(llvm::Module &M,
                      PrecalculationAnalysisImpl &precompute_analyis_result)
      : M(M), precompute_analyis_result(precompute_analyis_result) {
    insert_precomputation();
  };

  llvm::Function *get_precompute_main() const { return precompute_main; };

private:
  llvm::Module &M;
  PrecalculationAnalysisImpl &precompute_analyis_result;
  llvm::Function *precompute_main;

  std::map<llvm::Function *, std::shared_ptr<PrecalculationFunctionCopy>>
      functions_copied;
  llvm::Function *get_global_re_init_function();
  llvm::Function *create_precompute_main(
      const std::shared_ptr<PrecalculationFunctionCopy> &entry_function);
  void insert_precomputation();
  void
  prune_function_copy(const std::shared_ptr<PrecalculationFunctionCopy> &func);
  void replace_exceptionless_invoke_with_call(
      const std::shared_ptr<PrecalculationFunctionCopy> &func);
  void replace_calls_in_copy(
      const std::shared_ptr<PrecalculationFunctionCopy> &func);
  void replace_usages_of_func_in_copy(
      const std::shared_ptr<PrecalculationFunctionCopy> &func);
  // returns nullptr if not
  std::shared_ptr<PrecalculationFunctionCopy>
  is_in_a_precompute_copy_func(llvm::Instruction *inst);
};

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
