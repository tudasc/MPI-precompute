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
#ifndef MPI_ASSERTION_CHECKING_VTABLEMANAGER_H
#define MPI_ASSERTION_CHECKING_VTABLEMANAGER_H

#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/User.h>

#include <map>
#include <set>

class PrecalculationAnalysis;

class VtableManager {
  // manages the vtables to use in function copies
public:
  explicit VtableManager(llvm::Module &M) : M(M){};

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

#endif // MPI_ASSERTION_CHECKING_VTABLEMANAGER_H
