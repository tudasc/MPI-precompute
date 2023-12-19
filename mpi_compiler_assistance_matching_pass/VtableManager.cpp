//
// Created by tim on 19.12.23.
//

#include "VtableManager.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>

using namespace llvm;

llvm::GlobalVariable *
VtableManager::get_vtable_from_ptr_user(llvm::User *vtable_value) {
  assert(isa<ConstantAggregate>(vtable_value) &&
         "non constant vtable for a class?");
  assert(vtable_value->hasOneUser() &&
         "What kind of vtable is defined multiple times?");
  auto *vtable_initializer = cast<Constant>(vtable_value);
  if (isa<ConstantArray>(vtable_value)) {
    vtable_initializer =
        dyn_cast<Constant>(vtable_value->getUniqueUndroppableUser());
  }
  assert(vtable_initializer && "Vtable with a non constant initializer?");
  assert(vtable_initializer->hasOneUser() &&
         "storing the vtable into several globals is not supported?");
  auto *vtable_global =
      dyn_cast<Value>(vtable_initializer->getUniqueUndroppableUser());

  // errs() << "Found Vtable:\n";
  // vtable_global->dump();
  if (not isa<GlobalValue>(vtable_global)) {

    auto *current_level_of_definition = cast<Constant>(vtable_global);
    while (not isa<GlobalValue>(current_level_of_definition)) {
      assert(current_level_of_definition->hasOneUser() &&
             "storing the vtable into several globals is not supported?");
      current_level_of_definition = cast<Constant>(
          current_level_of_definition->getUniqueUndroppableUser());
    }
    // these special llvm magic globals are not touched
    // we will just use the normal initializers
    // as everything of this will be called before main actually starts
    assert(current_level_of_definition->getName().equals("llvm.global_ctors") ||
           current_level_of_definition->getName().equals("llvm.global_dtors"));
    return nullptr;
  }

  assert(dyn_cast<GlobalVariable>(vtable_global) &&
         "Vtable is not defined as a global?");
  return dyn_cast<GlobalVariable>(vtable_global);
}

void VtableManager::register_function_copy(llvm::Function *old_F,
                                           llvm::Function *new_F) {
  old_new_func_map.insert(std::make_pair(old_F, new_F));
  new_funcs.insert(new_F);
}

void VtableManager::perform_vtable_change_in_copies() {
  std::map<GlobalValue *, GlobalValue *> old_new_vtable_map;

  // collect all the vtables that need some changes
  for (auto pair : old_new_func_map) {

    auto old_func = pair.first;
    auto new_func = pair.second;

    for (auto u : old_func->users()) {
      if (isa<ConstantAggregate>(u)) {
        auto vtable_global = get_vtable_from_ptr_user(u);
        auto *vtable_value = dyn_cast<Constant>(u);
        // if get_vtable_from_ptr_user returns nullptr
        // we found one of the llvm special "vtables" @llvm.global_ctors
        // we will not change this (the application needs the full
        // initialization not just the parts relevant for tag computing)
        if (vtable_global && old_new_vtable_map.find(vtable_global) ==
                                 old_new_vtable_map.end()) {
          old_new_vtable_map.insert(
              // get the new vtable
              std::make_pair(vtable_global, get_replaced_vtable(vtable_value)));
        } else {
          // nothing to do, the first call will generate the full replaced
          // vtable, even if multiple entries in it needs to be replaced
        }
      }
    }
  }

  for (auto pair : old_new_vtable_map) {
    auto vtable_global_old = pair.first;
    auto *vtable_global_new = pair.second;

    // collections of uses to replace
    std::vector<std::tuple<Instruction *, Value *, Value *>> to_replace;
    for (auto *u : vtable_global_old->users()) {
      // only replace if in copy
      if (auto *inst = dyn_cast<Instruction>(u)) {
        if (new_funcs.find(inst->getFunction()) != new_funcs.end()) {
          to_replace.push_back(
              std::make_tuple(inst, vtable_global_old, vtable_global_new));
        }
      } else if (auto *constant = dyn_cast<ConstantExpr>(u)) {
        //  a use of a vtable entry
        // we need to replace the first operand with the new vtable, all other
        // operands stay the same
        std::vector<Constant *> operands;
        for (auto &op : constant->operands()) {
          operands.push_back(cast<Constant>(&op));
        }
        assert(operands[0] == vtable_global_old);
        operands[0] = vtable_global_new;
        Value *getelemptr_copy = constant->getWithOperands(operands);
        // all usages of this in copied functions
        for (auto uu : constant->users()) {
          if (auto *inst_uu = dyn_cast<Instruction>(uu)) {
            if (new_funcs.find(inst_uu->getFunction()) != new_funcs.end()) {
              to_replace.push_back(
                  std::make_tuple(inst_uu, constant, getelemptr_copy));
            }
          } else {
            errs() << "unknown use of vtable access :\n";
            u->dump();
            uu->dump();
            assert(false);
          }
        }
      } else {
        errs() << "unknown use of vtable:\n";
        u->dump();
        assert(false);
      }
    }
    // and replace
    for (auto triple : to_replace) {
      auto *inst = std::get<0>(triple);
      auto *from = std::get<1>(triple);
      auto *to = std::get<2>(triple);
      inst->replaceUsesOfWith(from, to);
      inst->dump();
    }

  } // end for each vtable
    // assert(false && "DEBUG");
}
GlobalVariable *
VtableManager::get_replaced_vtable(llvm::User *vtable_value_as_use) {
  auto vtable_global = get_vtable_from_ptr_user(vtable_value_as_use);

  auto *vtable_value = dyn_cast<ConstantArray>(vtable_value_as_use);
  assert(vtable_value && "The vtable is not a constant Array?");

  std::string name_of_copy =
      vtable_global->getName().str() + "_copy_for_precalc";

  // copy the vtable
  // auto *new_vtable_global = cast<GlobalVariable>(
  //    M.getOrInsertGlobal(name_of_copy, vtable_global->getType()));
  // new_vtable_global->setInitializer(nullptr);

  std::vector<Constant *> new_vtable;
  errs() << "old vtable:\n";
  vtable_global->dump();
  vtable_global->getType()->dump();

  for (unsigned int i = 0; i < vtable_value->getNumOperands(); ++i) {
    auto vtable_entry = vtable_value->getOperand(i);
    if (auto *func = dyn_cast<Function>(vtable_entry)) {
      if (old_new_func_map.find(func) != old_new_func_map.end()) {
        new_vtable.push_back(old_new_func_map[func]);
      } else {
        // this function was not copied: it will not be called during precalc
        new_vtable.push_back(Constant::getNullValue(vtable_entry->getType()));
      }
    } else {
      // keep old value (may be null or a special pure virtual value)
      new_vtable.push_back(vtable_entry);
    }
  }

  auto *new_vtable_value =
      ConstantArray::get(vtable_value->getType(), new_vtable);
  auto *old_vtable_initializer =
      cast<ConstantStruct>(vtable_value->getUniqueUndroppableUser());
  assert(old_vtable_initializer->getType()->getNumElements() ==
         1); // only the vtable array itself is the element
  auto *new_vtable_initializer =
      ConstantStruct::get(old_vtable_initializer->getType(), new_vtable_value);

  auto *new_vtable_global = new GlobalVariable(
      M, new_vtable_initializer->getType(), true, vtable_global->getLinkage(),
      new_vtable_initializer, name_of_copy, vtable_global);

  new_vtable_global->copyAttributesFrom(vtable_global);

  errs() << "new vtable:\n";
  new_vtable_global->dump();
  new_vtable_global->getType()->dump();

  return new_vtable_global;
}
