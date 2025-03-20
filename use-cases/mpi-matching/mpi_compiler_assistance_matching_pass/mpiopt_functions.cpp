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
#include "mpiopt_functions.h"
#include "implementation_specific.h"
#include "mpi_functions.h"
#include <assert.h>

#include "llvm/IR/InstrTypes.h"
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

void add_mpi_info_functions(llvm::Module &M) {
  auto mpi_func = get_mpi_functions(M);
  auto *mpi_implementation_specifics = ImplementationSpecifics::get_instance();

  if (mpi_func->mpi_info_create == nullptr) {
    auto fntype = FunctionType::get(
        Type::getInt32Ty(M.getContext()),
        {mpi_implementation_specifics->mpi_info->getPointerTo()}, false);

    mpi_func->mpi_info_create =
        cast<Function>(M.getOrInsertFunction("MPI_Info_create", fntype)
                           .getCallee()
                           ->stripPointerCasts());
  }
  if (mpi_func->mpi_info_free == nullptr) {
    auto fntype = FunctionType::get(
        Type::getInt32Ty(M.getContext()),
        {mpi_implementation_specifics->mpi_info->getPointerTo()}, false);

    mpi_func->mpi_info_free =
        cast<Function>(M.getOrInsertFunction("MPI_Info_free", fntype)
                           .getCallee()
                           ->stripPointerCasts());
  }
  if (mpi_func->mpi_info_set == nullptr) {
    auto fntype = FunctionType::get(Type::getInt32Ty(M.getContext()),
                                    {mpi_implementation_specifics->mpi_info,
                                     Type::getInt8PtrTy(M.getContext()),
                                     Type::getInt8PtrTy(M.getContext())},
                                    false);

    mpi_func->mpi_info_set =
        cast<Function>(M.getOrInsertFunction("MPI_Info_set", fntype)
                           .getCallee()
                           ->stripPointerCasts());
  }
}

struct mpiopt_functions *mpiopt_funct = nullptr;

struct mpiopt_functions *get_mpiopt_functions(llvm::Module &M) {

  if (mpiopt_funct)
    return mpiopt_funct;

  auto mpi_func = get_mpi_functions(M);

  struct mpiopt_functions *mpiopt_funct = new struct mpiopt_functions;
  auto *mpi_implementation_specifics = ImplementationSpecifics::get_instance();

  // construct the optimized version of functions, if original functions where
  // used:

  if (mpi_func->mpi_wait) {
    mpiopt_funct->mpi_wait = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Wait",
                              mpi_func->mpi_wait->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_waitall) {
    mpiopt_funct->mpi_waitall = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Waitall",
                              mpi_func->mpi_waitall->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_waitany) {
    mpiopt_funct->mpi_waitany = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Waitany",
                              mpi_func->mpi_waitany->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_waitsome) {
    mpiopt_funct->mpi_waitsome = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Waitsome",
                              mpi_func->mpi_waitsome->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_test) {
    mpiopt_funct->mpi_test = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Test",
                              mpi_func->mpi_test->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_testall) {
    mpiopt_funct->mpi_testall = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Testall",
                              mpi_func->mpi_testall->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_testany) {
    mpiopt_funct->mpi_testany = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Testany",
                              mpi_func->mpi_testany->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_testsome) {
    mpiopt_funct->mpi_testsome = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Testsome",
                              mpi_func->mpi_testsome->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_start) {
    mpiopt_funct->mpi_start = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Start",
                              mpi_func->mpi_start->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_startall) {
    mpiopt_funct->mpi_startall = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Startall",
                              mpi_func->mpi_startall->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_send_init) {
    mpiopt_funct->mpi_send_init = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Send_init",
                              mpi_func->mpi_send_init->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }

  if (mpi_func->mpi_recv_init) {
    mpiopt_funct->mpi_recv_init = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Recv_init",
                              mpi_func->mpi_recv_init->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_request_free) {
    mpiopt_funct->mpi_request_free = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Request_free",
                              mpi_func->mpi_request_free->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }

  if (mpi_func->mpi_send_init) {
    auto orig_fn_type = mpi_func->mpi_send_init->getFunctionType();
    std::vector<Type *> params;
    std::copy(orig_fn_type->param_begin(), orig_fn_type->param_end(),
              std::back_inserter(params));
    params.push_back(mpi_implementation_specifics->mpi_info);

    auto new_fntype =
        FunctionType::get(orig_fn_type->getReturnType(), params, false);
    mpiopt_funct->mpi_send_init_info =
        cast<Function>(M.getOrInsertFunction("MPIOPT_Send_init_x", new_fntype)
                           .getCallee()
                           ->stripPointerCasts());
  }
  if (mpi_func->mpi_recv_init) {
    auto orig_fn_type = mpi_func->mpi_recv_init->getFunctionType();
    std::vector<Type *> params;
    std::copy(orig_fn_type->param_begin(), orig_fn_type->param_end(),
              std::back_inserter(params));
    params.push_back(mpi_implementation_specifics->mpi_info);

    auto new_fntype =
        FunctionType::get(orig_fn_type->getReturnType(), params, false);
    mpiopt_funct->mpi_recv_init_info =
        cast<Function>(M.getOrInsertFunction("MPIOPT_Recv_init_x", new_fntype)
                           .getCallee()
                           ->stripPointerCasts());
  }

  // construct the init and finish functions, if necessary:
  if (mpi_func->mpi_init || mpi_func->mpi_init_thread ||
      mpi_func->mpi_finalize) {
    // void funcs that do not have params
    auto *ftype = FunctionType::get(Type::getVoidTy(M.getContext()), false);
    mpiopt_funct->init =
        cast<Function>(M.getOrInsertFunction("MPIOPT_INIT", ftype, {})
                           .getCallee()
                           ->stripPointerCasts());
    mpiopt_funct->finalize =
        cast<Function>(M.getOrInsertFunction("MPIOPT_FINALIZE", ftype, {})
                           .getCallee()
                           ->stripPointerCasts());
  }

  return mpiopt_funct;
}
