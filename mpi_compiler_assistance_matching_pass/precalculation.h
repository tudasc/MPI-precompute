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
#ifndef MACH_PRECALCULATIONS_H_
#define MACH_PRECALCULATIONS_H_

#include "devirt_analysis.h"
#include "ptr_info.h"
#include "taintedValue.h"
#include <numeric>
#include <utility>

#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/Cloning.h"

class Precalculations;

class VtableManager {
  // manages the vtables to use in function copies
public:
  VtableManager(llvm::Module &M) : M(M){};

  void register_function_copy(llvm::Function *old_F, llvm::Function *new_F);
  // once all functions have been registered
  void perform_vtable_change_in_copies();

private:
  llvm::Module &M;

  std::map<llvm::Function *, llvm::Function *> old_new_func_map;
  // just for ease of programming, it is also contained in the map above:
  std::set<llvm::Function *> new_funcs;

  llvm::GlobalVariable *get_replaced_vtable(llvm::User *vtable_value);

  static llvm::GlobalVariable *
  get_vtable_from_ptr_user(llvm::User *vtable_value);
};


class FunctionToPrecalculate {
public:
  FunctionToPrecalculate(llvm::Function *F) : F_orig(F) {
    assert(not F->isDeclaration() && "Cannot analyze external function");
  };
  void add_relevant_args(const std::set<unsigned int> &new_args_to_use) {
    std::copy(new_args_to_use.begin(), new_args_to_use.end(),
              std::inserter(args_to_use, args_to_use.begin()));
  }

  std::set<unsigned int> args_to_use = {};
  llvm::Function *F_orig;
  llvm::Function *F_copy = nullptr;
  llvm::ValueToValueMapTy old_new_map;
  std::map<llvm::Value *, llvm::Value *> new_to_old_map;
  llvm::ClonedCodeInfo *cloned_code_info = nullptr; // currently we dont need it

  void initialize_copy();
};

// TODO different interface
class Precalculations {
public:
  Precalculations(llvm::Module &M, llvm::Function *entry_point)
      : M(M), entry_point(entry_point), virtual_call_sites(DevirtAnalysis(M)) {
    find_functions_called_indirect();
  };

  void add_precalculations(const std::vector<llvm::CallBase *> &to_precompute);

public:
  llvm::Module &M;
  llvm::Function *entry_point;

  DevirtAnalysis virtual_call_sites;

  std::vector<llvm::CallBase *> to_replace_with_envelope_register;
  std::set<std::shared_ptr<FunctionToPrecalculate>> functions_to_include;
  std::set<std::shared_ptr<TaintedValue>> tainted_values;

  std::shared_ptr<TaintedValue>
  insert_tainted_value(llvm::Value *v,
                       std::shared_ptr<TaintedValue> from = nullptr);

  std::shared_ptr<TaintedValue> insert_tainted_value(llvm::Value *v,
                                                     TaintReason reason);
  void remove_tainted_value(const std::shared_ptr<TaintedValue> &value_info);

  std::shared_ptr<FunctionToPrecalculate>
  insert_functions_to_include(llvm::Function *func);

  // TODO we need some kind of heuristic to check if precalculation of all msg
  // tags seems to be worth it
  //  or if e.g. for some reason a compute heavy loop was included as well

  std::set<llvm::Function *> functions_that_may_be_called_indirect;

  void find_all_tainted_vals();
  void find_functions_called_indirect();
  // we need a function to re-initialize all globals that may be overwritten
  llvm::Function *get_global_re_init_function();

  void print_analysis_result_remarks();
  void debug_printings();

  void visit_val(std::shared_ptr<TaintedValue> v);
  void visit_arg(std::shared_ptr<TaintedValue> arg_info);

  void visit_load(const std::shared_ptr<TaintedValue> &load_info);
  void visit_store(const std::shared_ptr<TaintedValue> &store_info);
  void visit_store_from_value(const std::shared_ptr<TaintedValue> &store_info);
  void visit_store_from_ptr(const std::shared_ptr<TaintedValue> &store_info);
  void visit_gep(const std::shared_ptr<TaintedValue> &gep_info);
  void visit_phi(const std::shared_ptr<TaintedValue> &phi_info);

  void visit_call(std::shared_ptr<TaintedValue> call_info);
  void visit_call_from_ptr(llvm::CallBase *call,
                           std::shared_ptr<TaintedValue> ptr);
  void visit_ptr_usages(std::shared_ptr<TaintedValue> ptr);

  void replace_allocation_call(llvm::CallBase *call);

  bool is_tainted(llvm::Value *v) {
    return std::find_if(tainted_values.begin(), tainted_values.end(),
                        [&v](const auto &vv) { return vv->v == v; }) !=
           tainted_values.end();
  }

  template <class container> unsigned int get_num_tainted(container vals) {
    return std::accumulate(
        vals.begin(), vals.end(), (unsigned int)0,
        [this](auto accu, auto v) { return accu + is_tainted(v); });
  };

  template <class container> bool are_all_tainted(container vals) {
    return get_num_tainted(vals) == vals.size();
  }

  template <class container> bool is_none_tainted(container vals) {
    return !std::accumulate(
        vals.begin(), vals.end(), false,
        [this](auto accu, auto v) { return accu || is_tainted(v); });
  };

  bool is_retval_of_call_used(llvm::CallBase *call);

  void taint_all_indirect_call_args(llvm::Function *func, unsigned int argNo,
                                    std::shared_ptr<TaintedValue> arg_info);
  void taint_all_indirect_calls(llvm::Function *func);

  void replace_calls_in_copy(std::shared_ptr<FunctionToPrecalculate> func);
  void
  replace_usages_of_func_in_copy(std::shared_ptr<FunctionToPrecalculate> func);
  void prune_function_copy(const std::shared_ptr<FunctionToPrecalculate> &func);

  bool is_invoke_necessary_for_control_flow(llvm::InvokeInst *invoke);

  void add_call_to_precalculation_to_main();

  std::vector<llvm::Function *> get_possible_call_targets(llvm::CallBase *call);
  void insert_necessary_control_flow(llvm::Value *v);
};

#endif // MACH_PRECALCULATIONS_H_