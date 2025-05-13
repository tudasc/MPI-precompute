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
#ifndef MACH_PRECALCULATIONS_IMPL_H_
#define MACH_PRECALCULATIONS_IMPL_H_

#include "Precompute_insertion.h"

#include <numeric>
#include <regex>
#include <utility>

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "VtableManager.h"
#include "analysis_results.h"
#include "devirt_analysis.h"
#include "mpi_functions.h"
#include "precalculation_function_analysis.h"

class PrecomputeInsertion; // such that include order doesn't matter

class PrecalculationAnalysis
    : private std::enable_shared_from_this<PrecalculationAnalysis> {
public:
  PrecalculationAnalysis(llvm::Module &M, llvm::Function *entry_point,
                         std::vector<llvm::Value *> to_precompute_value,
                         std::vector<llvm::Instruction *> to_precompute_cfg)
      : mpi_func(get_mpi_functions(M)), M(M), entry_point(entry_point),
        to_precompute_value(std::move(to_precompute_value)),
        to_precompute_cfg(std::move(to_precompute_cfg)) {

    analyze();
  };

  ~PrecalculationAnalysis() = default;

  std::set<std::shared_ptr<PrecalculationFunctionAnalysis>>
  getFunctionsToInclude() const;

  std::shared_ptr<TaintedValue> get_taint_info(llvm::Value *v) const {
    assert(is_tainted(v));
    return *std::find_if(tainted_values.begin(), tainted_values.end(),
                         [&v](const auto &vv) { return vv->v == v; });
  }

  bool is_func_included_in_precompute(llvm::Function *F) const {
    return function_analysis.at(F)->include_in_precompute;
  }

  std::shared_ptr<PrecalculationFunctionAnalysis>
  get_function_analysis(llvm::Function *F) const {
    return function_analysis.at(F);
  }

  bool is_tainted(llvm::Value *v) const {
    return std::find_if(tainted_values.begin(), tainted_values.end(),
                        [&v](const auto &vv) { return vv->v == v; }) !=
           tainted_values.end();
  }

  bool is_included_in_precompute(llvm::Value *v) const {
    return std::find_if(tainted_values.begin(), tainted_values.end(),
                        [&v](const auto &vv) {
                          return (vv->v == v && vv->isIncludeInPrecompute());
                        }) != tainted_values.end();
  }

  template <class container>
  unsigned int get_num_tainted(container vals) const {
    return std::accumulate(
        vals.begin(), vals.end(), (unsigned int)0,
        [this](auto accu, auto v) { return accu + is_tainted(v); });
  };

  template <class container> bool are_all_tainted(container vals) const {
    return get_num_tainted(vals) == vals.size();
  }

  template <class container> bool is_none_tainted(container vals) const {
    return !std::accumulate(
        vals.begin(), vals.end(), false,
        [this](auto accu, auto v) { return accu || is_tainted(v); });
  };

  bool is_retval_of_call_needed(llvm::CallBase *call) const;

  bool is_invoke_necessary_for_control_flow(llvm::InvokeInst *invoke) const;

  bool is_invoke_exception_case_needed(llvm::InvokeInst *invoke) const;

  bool can_except_in_precompute(llvm::CallBase *call) const;

  std::vector<llvm::Function *>
  get_possible_call_targets(llvm::CallBase *call) const;

  llvm::Function *get_entry_point() const { return entry_point; }

  std::vector<llvm::Value *> get_values_to_precompute() const {
    return to_precompute_value;
  }
  std::vector<llvm::Instruction *> get_locations_to_precompute() const {
    return to_precompute_cfg;
  }

private:
  std::shared_ptr<PrecomputeInsertion> insertion_information = nullptr;
  std::map<llvm::Function *, std::shared_ptr<PrecalculationFunctionAnalysis>>
      function_analysis;

  std::map<llvm::Value *, llvm::Value *> precomputed_values_map;

  void analyze();
  void analyze_functions();

  std::unique_ptr<struct mpi_functions> mpi_func;

  llvm::Module &M;
  llvm::Function *entry_point;

  std::vector<llvm::Value *> to_precompute_value;
  std::vector<llvm::Instruction *> to_precompute_cfg;

  std::set<std::shared_ptr<TaintedValue>> tainted_values;

  void include_value_in_precompute(const std::shared_ptr<TaintedValue> &);

  std::shared_ptr<TaintedValue>
  insert_tainted_value(llvm::Value *v,
                       const std::shared_ptr<TaintedValue> &from = nullptr,
                       bool needed_from = true);

  std::shared_ptr<TaintedValue> insert_tainted_value(llvm::Value *v,
                                                     TaintReason reason);

  void insert_function_to_include(llvm::Function *func);

  void find_all_tainted_vals();

  // we need a function to re-initialize all globals that may be overwritten

  void print_analysis_result_remarks();

  void debug_printings();

  bool is_store_important(llvm::Instruction *inst,
                          const std::shared_ptr<PtrUsageInfo> &ptr_info);

  bool
  store_happens_after_all_loads(llvm::Instruction *inst,
                                const std::shared_ptr<PtrUsageInfo> &ptr_info);

  void get_all_transitive_insts(std::set<llvm::Instruction *> &instrs);

  // materialize call
  void include_call_to_std(const std::shared_ptr<TaintedValue> &call_info);

  bool
  is_ptr_usage_in_std_read(llvm::CallBase *call,
                           const std::shared_ptr<TaintedValue> &ptr_arg_info);

  bool
  is_ptr_usage_in_std_write(llvm::CallBase *call,
                            const std::shared_ptr<TaintedValue> &ptr_arg_info);
  bool is_ptr_usage_in_std_indirect(
      llvm::CallBase *call, const std::shared_ptr<TaintedValue> &ptr_arg_info);

  void insert_necessary_control_flow(llvm::Value *v);

  void visit_call_for_retval(const std::shared_ptr<TaintedValue> &call_info);

  void
  visit_invoke_for_exception(const std::shared_ptr<TaintedValue> &call_info);

  bool check_if_call_should_be_included(
      const std::shared_ptr<TaintedValue> &call_info);

  // visit the different type of values
  void visit_val(const std::shared_ptr<TaintedValue> &v);

  void visit_arg(const std::shared_ptr<TaintedValue> &arg_info);

  void visit_load(const std::shared_ptr<TaintedValue> &load_info,
                  const std::shared_ptr<TaintedValue> &ptr_operand);

  void visit_store(const std::shared_ptr<TaintedValue> &store_info,
                   llvm::Value *ptr, llvm::Value *store_val);

  void visit_gep(const std::shared_ptr<TaintedValue> &gep_info);

  void visit_phi(const std::shared_ptr<TaintedValue> &phi_info);

  void visit_call(const std::shared_ptr<TaintedValue> &call_info);

  void visit_call_to_parallel(const std::shared_ptr<TaintedValue> &call_info);

  void visit_call_from_ptr(llvm::CallBase *call,
                           const std::shared_ptr<TaintedValue> &ptr);

  void visit_ptr_usages(const std::shared_ptr<TaintedValue> &ptr);

  void visit_ptr_load(const std::shared_ptr<TaintedValue> &ptr,
                      llvm::Instruction *inst);

  void visit_ptr_store(const std::shared_ptr<TaintedValue> &ptr,
                       llvm::Instruction *inst);

  void visit_ptr_ret(const std::shared_ptr<TaintedValue> &ptr,
                     llvm::ReturnInst *ret);

  void handle_vararg_ptr_alias(const std::shared_ptr<TaintedValue> &ptr,
                               llvm::Function *func);

  bool
  visit_ptr_insertvalue_recursive_impl(const std::shared_ptr<TaintedValue> &ptr,
                                       llvm::ArrayRef<unsigned> insert_idx,
                                       llvm::Instruction *aggregate_inst);
  void visit_ptr_insertvalue(const std::shared_ptr<TaintedValue> &ptr,
                             llvm::InsertValueInst *insert_value_inst);

  bool visit_ptr_insertelement_recursive_impl(
      const std::shared_ptr<TaintedValue> &ptr, llvm::Value *insert_idx,
      llvm::Instruction *aggregate_inst);

  void visit_ptr_insertelement(const std::shared_ptr<TaintedValue> &ptr,
                               llvm::InsertElementInst *insertelem_inst);
};

#endif // MACH_PRECALCULATIONS_IMPL_H_
