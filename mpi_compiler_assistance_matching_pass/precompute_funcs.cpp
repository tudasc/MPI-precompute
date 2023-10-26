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

#include "precompute_funcs.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"

#include "llvm/Support/raw_ostream.h"
#include <mpi.h>

using namespace llvm;

PrecomputeFunctions *PrecomputeFunctions::instance = nullptr;

PrecomputeFunctions::PrecomputeFunctions(Module &M) {

  auto *ftype = FunctionType::get(
      Type::getVoidTy(M.getContext()),
      {Type::getInt32Ty(M.getContext()), Type::getInt32Ty(M.getContext())},
      false);

  register_precomputed_value = cast<Function>(
      M.getOrInsertFunction("register_precomputed_value", ftype, {})
          .getCallee()
          ->stripPointerCasts());

  ftype = FunctionType::get(Type::getVoidTy(M.getContext()), {}, false);

  init_precompute_lib =
      cast<Function>(M.getOrInsertFunction("init_precompute_lib", ftype, {})
                         .getCallee()
                         ->stripPointerCasts());
  finish_precomputation =
      cast<Function>(M.getOrInsertFunction("finish_precomputation", ftype, {})
                         .getCallee()
                         ->stripPointerCasts());

  ftype = FunctionType::get(Type::getInt8PtrTy(M.getContext()),
                            {Type::getInt64Ty(M.getContext())}, false);

  allocate_memory = cast<Function>(
      M.getOrInsertFunction("allocate_memory_in_precompute", ftype, {})
          .getCallee()
          ->stripPointerCasts());
}
