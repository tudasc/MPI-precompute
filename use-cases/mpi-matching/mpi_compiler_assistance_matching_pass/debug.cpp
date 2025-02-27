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
#include "debug.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"

using namespace llvm;

void add_debug_printfs_to_precalculation(llvm::Function *func) {

  auto *M = func->getParent();
  auto *ftype = FunctionType::get(Type::getVoidTy(M->getContext()),
                                  Type::getInt8PtrTy(M->getContext()), true);
  auto printf_func = M->getOrInsertFunction("printf", ftype);

  for (auto &bb : *func) {
    IRBuilder<> builder = IRBuilder<>(bb.getTerminator());
    auto s = "func: " + func->getName().str() +
             " block: " + bb.getName().str() + "\n";
    builder.CreateCall(printf_func, builder.CreateGlobalString(s));

    if (auto *ret = dyn_cast<ReturnInst>(bb.getTerminator())) {
      IRBuilder<> builder = IRBuilder<>(ret);
      auto s = "Return from func: " + func->getName().str() + " from block " +
               bb.getName().str() + "\n";
      builder.CreateCall(printf_func, builder.CreateGlobalString(s));
    }

    if (auto *sw = dyn_cast<SwitchInst>(bb.getTerminator())) {
      IRBuilder<> builder = IRBuilder<>(sw);
      auto s = "Switch Value: %d In BB: " + bb.getName().str() + "\n";
      builder.CreateCall(printf_func,
                         {builder.CreateGlobalString(s), sw->getCondition()});
    }

    for (auto &inst : bb) {
      if (auto *store = dyn_cast<StoreInst>(&inst)) {
        if (store->getPointerOperand()->getName() == "ImplId") {
          IRBuilder<> builder = IRBuilder<>(store);
          auto s = "SStore To ImplID: %d In BB: " + bb.getName().str() + "\n";
          builder.CreateCall(printf_func, {builder.CreateGlobalString(s),
                                           store->getValueOperand()});
        }
      }
    }
  }
}
