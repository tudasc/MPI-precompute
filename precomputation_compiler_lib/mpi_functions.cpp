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
#include "mpi_functions.h"
#include <assert.h>

#include "llvm/IR/InstrTypes.h"
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

std::vector<CallBase *> gather_all_calls(Function *f) {
  std::vector<CallBase *> result;
  if (f) {
    for (auto user : f->users()) {
      if (CallBase *call = dyn_cast<CallBase>(user)) {
        if (call->getCalledFunction() == f) {

          result.push_back(call);
        } else {
          call->dump();
          errs() << "\nWhy do you do that?\n";
        }
      }
    }
  }
  return result;
}

struct mpi_functions *mpi_funcs = nullptr;

struct mpi_functions *get_used_mpi_functions(llvm::Module &M) {

  if (mpi_funcs)
    return mpi_funcs;

  struct mpi_functions *result = new struct mpi_functions;
  assert(result != nullptr);

  for (auto it = M.begin(); it != M.end(); ++it) {
    Function *f = &*it;
    if (f->getName() == "MPI_Init") {
      result->mpi_init = f;

    } else if (f->getName() == "MPI_Init_thread") {
      result->mpi_init_thread = f;
    } else if (f->getName() == "MPI_Comm_rank") {
      result->mpi_comm_rank = f;
    } else if (f->getName() == "MPI_Comm_size") {
      result->mpi_comm_size = f;

      // sync functions:
    } else if (f->getName() == "MPI_Finalize") {
      result->mpi_finalize = f;

    } else if (f->getName() == "MPI_Barrier") {
      result->mpi_barrier = f;

    } else if (f->getName() == "MPI_Ibarrier") {
      result->mpi_Ibarrier = f;

    } else if (f->getName() == "MPI_Allreduce") {
      result->mpi_allreduce = f;

    } else if (f->getName() == "MPI_Iallreduce") {
      result->mpi_Iallreduce = f;

    } else if (f->getName() == "MPI_Wtime") {
      result->mpi_wtime = f;
    }

    // different sending modes:
    else if (f->getName() == "MPI_Send") {
      result->mpi_send = f;

    } else if (f->getName() == "MPI_Bsend") {
      result->mpi_Bsend = f;

    } else if (f->getName() == "MPI_Ssend") {
      result->mpi_Ssend = f;

    } else if (f->getName() == "MPI_Rsend") {
      result->mpi_Rsend = f;

    } else if (f->getName() == "MPI_Isend") {
      result->mpi_Isend = f;

    } else if (f->getName() == "MPI_Ibsend") {
      result->mpi_Ibsend = f;

    } else if (f->getName() == "MPI_Issend") {
      result->mpi_Issend = f;

    } else if (f->getName() == "MPI_Irsend") {
      result->mpi_Irsend = f;

    } else if (f->getName() == "MPI_Sendrecv") {
      result->mpi_Sendrecv = f;

    } else if (f->getName() == "MPI_Recv") {
      result->mpi_recv = f;

    } else if (f->getName() == "MPI_Irecv") {
      result->mpi_Irecv = f;

      // Other MPI functions, that themselves may not yield another conflict
    } else if (f->getName() == "MPI_Buffer_detach") {
      result->mpi_buffer_detach = f;

    } else if (f->getName() == "MPI_Test") {
      result->mpi_test = f;
    } else if (f->getName() == "MPI_Testall") {
      result->mpi_testall = f;
    } else if (f->getName() == "MPI_Testany") {
      result->mpi_testany = f;
    } else if (f->getName() == "MPI_Testsome") {
      result->mpi_testsome = f;
    } else if (f->getName() == "MPI_Wait") {
      result->mpi_wait = f;
    } else if (f->getName() == "MPI_Waitall") {
      result->mpi_waitall = f;
    } else if (f->getName() == "MPI_Waitany") {
      result->mpi_waitany = f;
    } else if (f->getName() == "MPI_Waitsome") {
      result->mpi_waitsome = f;
    } else if (f->getName() == "MPI_Start") {
      result->mpi_start = f;
    } else if (f->getName() == "MPI_Startall") {
      result->mpi_startall = f;
    } else if (f->getName() == "MPI_Recv_init") {
      result->mpi_recv_init = f;

    } else if (f->getName() == "MPI_Send_init") {
      result->mpi_send_init = f;

    } else if (f->getName() == "MPI_Request_free") {
      result->mpi_request_free = f;
    } else if (f->getName() == "MPI_Info_create") {
      result->mpi_info_create = f;
    } else if (f->getName() == "MPI_Info_set") {
      result->mpi_info_set = f;
    } else if (f->getName() == "MPI_Info_free") {
      result->mpi_info_free = f;
    }
  }

  mpi_funcs = result;
  return result;
}

struct mpi_functions *mpi_func = nullptr;

struct mpi_functions *get_mpi_functions(llvm::Module &M) {
  if (!mpi_func) {
    mpi_func = get_used_mpi_functions(M);
  }
  return mpi_func;
}

bool is_mpi_initialized() {
  if (mpi_func->mpi_init != nullptr) {
    return mpi_func->mpi_init->getNumUses() > 0;
  } else if (mpi_func->mpi_init_thread != nullptr) {
    return mpi_func->mpi_init_thread->getNumUses() > 0;
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
