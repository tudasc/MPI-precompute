//
// Created by tim on 19.12.23.
//

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
