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
#include "precalculation_function_analysis.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <map>

class PrecalculationAnalysis; // such that include order doesn't matter
class PrecalculationFunctionAnalysis;


#include <llvm/IR/Module.h>

class PrecalculationFunctionCopy;

class PrecomputeInsertion {
public:
  PrecomputeInsertion(llvm::Module &M,
      const std::shared_ptr<PrecalculationAnalysis> &precompute_analyis_result,
      bool replace_allocation = true)
      : M(M), precompute_analyis_result(precompute_analyis_result),
        replace_allocation(replace_allocation) {
    insert_precomputation();
  };

  // TODO documentation
  //  constructor performs the analysis and generated a slice
  //  than use get_precomputed_value to do what is necessary for the
  //  precomputed values

  // TODO offer helper functions to add calls to precompute backend library

  llvm::Function *get_precompute_main() const { return precompute_main; };

  bool is_func_part_of_precompute_phase(llvm::Function *const F) const {
    assert(F != nullptr);

    return (std::find_if(functions_copied.begin(), functions_copied.end(),
                         [&F](const auto &it) {
                           return it.second->F_copy == F;
                         }) != functions_copied.end());
  }

  // this removes all values in to_precompute_cfg
  void clean_precompute();

  // accessor (use it to modify the program slice if necessary)
  llvm::Value *get_precomputed_value(llvm::Value *v) const {
    return precomputed_values_map.at(v);
  };

private:
  llvm::Module &M;
  std::shared_ptr<const PrecalculationAnalysis> precompute_analyis_result;
  // true if allocations should be managed (and freed after precompute) by
  // precompute backend library
  bool replace_allocation;


  llvm::Function *precompute_main;

  std::map<llvm::Value *, llvm::Value *> precomputed_values_map;

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
  void build_precomputed_values_map();
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
