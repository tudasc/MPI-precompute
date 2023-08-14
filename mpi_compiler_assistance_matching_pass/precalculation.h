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
  // just for ease of programming, it is also contained in the map above
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
      : M(M), entry_point(entry_point) {
    find_functionTypes_called_indirect();
  };

  void add_precalculations(const std::vector<llvm::CallBase *> &to_precompute);

public:
  llvm::Module &M;
  llvm::Function *entry_point;

  std::vector<llvm::CallBase *> to_replace_with_envelope_register;
  std::set<std::shared_ptr<FunctionToPrecalculate>> functions_to_include;
  std::set<llvm::Value *> tainted_values;
  std::set<llvm::BasicBlock *> tainted_blocks;
  std::set<llvm::Value *> visited_values;

  void insert_tainted_value(llvm::Value *v);

  std::shared_ptr<FunctionToPrecalculate>
  insert_functions_to_include(llvm::Function *func);
  // TODO we need some kind of heuristic to check if precalculation of all msg
  // tags seems to be worth it
  //  or if e.g. for some reason a compute heavy loop was included as well

  std::set<llvm::FunctionType *> fn_types_with_indirect_calls;

  void find_all_tainted_vals();
  void find_functionTypes_called_indirect();
  void visit_all_indirect_calls_for_FnType(llvm::FunctionType *fntype);
  void visit_all_indirect_call_args_for_FnType(llvm::FunctionType *fntype,
                                               unsigned int argNo);
  void visit_val(llvm::Value *v);
  void visit_val(llvm::AllocaInst *alloca);
  void visit_val(llvm::PHINode *phi);
  void visit_val(llvm::StoreInst *store);
  void visit_val(llvm::Argument *arg);
  void visit_val(llvm::CallBase *call);
  void visit_call_from_ptr(llvm::CallBase *call, llvm::Value *ptr);
  void visit_ptr(llvm::Value *ptr);

  bool is_retval_of_call_used(llvm::CallBase *call) const;

  void replace_calls_in_copy(std::shared_ptr<FunctionToPrecalculate> func);
  void
  replace_usages_of_func_in_copy(std::shared_ptr<FunctionToPrecalculate> func);
  void prune_function_copy(const std::shared_ptr<FunctionToPrecalculate> &func);

  bool is_invoke_necessary_for_control_flow(llvm::InvokeInst *invoke) const;

  void add_call_to_precalculation_to_main();
};

#endif // MACH_PRECALCULATIONS_H_