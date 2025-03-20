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

struct mpiopt_functions *get_mpiopt_functions(llvm::Module &M) {

  struct mpiopt_functions *result = new struct mpiopt_functions;
  assert(result != nullptr);
  auto *mpi_implementation_specifics = ImplementationSpecifics::get_instance();

  // construct the optimized version of functions, if original functions where
  // used:

  if (mpi_func->mpi_wait) {
    result->mpi_wait = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Wait",
                              mpi_func->mpi_wait->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_waitall) {
    result->mpi_waitall = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Waitall",
                              mpi_func->mpi_waitall->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_waitany) {
    result->mpi_waitany = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Waitany",
                              mpi_func->mpi_waitany->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_waitsome) {
    result->mpi_waitsome = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Waitsome",
                              mpi_func->mpi_waitsome->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_test) {
    result->mpi_test = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Test",
                              mpi_func->mpi_test->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_testall) {
    result->mpi_testall = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Testall",
                              mpi_func->mpi_testall->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_testany) {
    result->mpi_testany = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Testany",
                              mpi_func->mpi_testany->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_testsome) {
    result->mpi_testsome = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Testsome",
                              mpi_func->mpi_testsome->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_start) {
    result->mpi_start = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Start",
                              mpi_func->mpi_start->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_startall) {
    result->mpi_startall = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Startall",
                              mpi_func->mpi_startall->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_send_init) {
    result->mpi_send_init = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Send_init",
                              mpi_func->mpi_send_init->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }

  if (mpi_func->mpi_recv_init) {
    result->mpi_recv_init = cast<Function>(
        M.getOrInsertFunction("MPIOPT_Recv_init",
                              mpi_func->mpi_recv_init->getFunctionType())
            .getCallee()
            ->stripPointerCasts());
  }
  if (mpi_func->mpi_request_free) {
    result->mpi_request_free = cast<Function>(
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
    result->mpi_send_init_info =
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
    result->mpi_recv_init_info =
        cast<Function>(M.getOrInsertFunction("MPIOPT_Recv_init_x", new_fntype)
                           .getCallee()
                           ->stripPointerCasts());
  }

  // construct the init and finish functions, if necessary:
  if (mpi_func->mpi_init || mpi_func->mpi_init_thread ||
      mpi_func->mpi_finalize) {
    // void funcs that do not have params
    auto *ftype = FunctionType::get(Type::getVoidTy(M.getContext()), false);
    result->init =
        cast<Function>(M.getOrInsertFunction("MPIOPT_INIT", ftype, {})
                           .getCallee()
                           ->stripPointerCasts());
    result->finalize =
        cast<Function>(M.getOrInsertFunction("MPIOPT_FINALIZE", ftype, {})
                           .getCallee()
                           ->stripPointerCasts());
  }

  return result;
}

bool is_mpi_used(struct mpi_functions *mpi_func) {

  if (mpi_func->mpi_init != nullptr) {
    return mpi_func->mpi_init->getNumUses() > 0;
  } else {
    return false;
  }
}

bool is_send_function(llvm::Function *f) {
  assert(f != nullptr);
  return f == mpi_func->mpi_send || f == mpi_func->mpi_Bsend ||
         f == mpi_func->mpi_Ssend || f == mpi_func->mpi_Rsend ||
         f == mpi_func->mpi_Isend || f == mpi_func->mpi_Ibsend ||
         f == mpi_func->mpi_Irsend || f == mpi_func->mpi_Issend ||
         f == mpi_func->mpi_Sendrecv || f == mpi_func->mpi_send_init;
}

bool is_recv_function(llvm::Function *f) {
  assert(f != nullptr);
  return f == mpi_func->mpi_recv || f == mpi_func->mpi_Irecv ||
         f == mpi_func->mpi_Sendrecv || f == mpi_func->mpi_recv_init;
}

Value *get_communicator_value(CallBase *mpi_call) {

  unsigned int total_num_args = 0;
  unsigned int communicator_arg_pos = 0;

  if (mpi_call->getCalledFunction() == mpi_func->mpi_send ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Bsend ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Ssend ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Rsend) {
    total_num_args = 6;
    communicator_arg_pos = 5;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_Isend) {
    total_num_args = 7;
    communicator_arg_pos = 5;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_recv ||
             mpi_call->getCalledFunction() == mpi_func->mpi_Irecv) {
    total_num_args = 7;
    communicator_arg_pos = 5;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_Sendrecv) {
    total_num_args = 12;
    communicator_arg_pos = 10;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_send_init ||
             mpi_call->getCalledFunction() == mpi_func->mpi_recv_init) {
    total_num_args = 7;
    communicator_arg_pos = 5;
  } else {
    errs() << mpi_call->getCalledFunction()->getName()
           << ": This MPI function is currently not supported\n";
    assert(false);
  }

  assert(mpi_call->arg_size() == total_num_args);

  return mpi_call->getArgOperand(communicator_arg_pos);
}

Value *get_src_value(CallBase *mpi_call, bool is_send) {

  unsigned int total_num_args = 0;
  unsigned int src_arg_pos = 0;

  if (mpi_call->getCalledFunction() == mpi_func->mpi_send ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Bsend ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Ssend ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Rsend) {
    assert(is_send);
    total_num_args = 6;
    src_arg_pos = 3;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_Isend) {
    assert(is_send);
    total_num_args = 7;
    src_arg_pos = 3;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_recv ||
             mpi_call->getCalledFunction() == mpi_func->mpi_Irecv) {
    assert(!is_send);
    total_num_args = 7;
    src_arg_pos = 3;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_Sendrecv) {
    total_num_args = 12;
    if (is_send)
      src_arg_pos = 3;
    else
      src_arg_pos = 8;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_send_init ||
             mpi_call->getCalledFunction() == mpi_func->mpi_recv_init) {
    total_num_args = 7;
    src_arg_pos = 3;
  } else {
    errs() << mpi_call->getCalledFunction()->getName()
           << ": This MPI function is currently not supported\n";
    assert(false);
  }

  assert(mpi_call->arg_size() == total_num_args);

  return mpi_call->getArgOperand(src_arg_pos);
}

Value *get_tag_value(CallBase *mpi_call, bool is_send) {

  unsigned int total_num_args = 0;
  unsigned int tag_arg_pos = 0;

  if (mpi_call->getCalledFunction() == mpi_func->mpi_send ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Bsend ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Ssend ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Rsend) {
    assert(is_send);
    total_num_args = 6;
    tag_arg_pos = 4;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_Isend) {
    assert(is_send);
    total_num_args = 7;
    tag_arg_pos = 4;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_recv ||
             mpi_call->getCalledFunction() == mpi_func->mpi_Irecv) {
    assert(!is_send);
    total_num_args = 7;
    tag_arg_pos = 4;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_Sendrecv) {
    total_num_args = 12;
    if (is_send)
      tag_arg_pos = 4;
    else
      tag_arg_pos = 9;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_send_init ||
             mpi_call->getCalledFunction() == mpi_func->mpi_recv_init) {
    total_num_args = 7;
    tag_arg_pos = 4;
  } else {
    errs() << mpi_call->getCalledFunction()->getName()
           << ": This MPI function is currently not supported\n";
    assert(false);
  }

  assert(mpi_call->arg_size() == total_num_args);

  return mpi_call->getArgOperand(tag_arg_pos);
}
