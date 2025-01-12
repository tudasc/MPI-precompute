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
#ifndef MACH_MPI_FUNCTIONS_H_
#define MACH_MPI_FUNCTIONS_H_

#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"

#include <set>

// global:
// will be init and destroyed in the Passes runOnModule function (equivalent to
// main)
extern struct mpi_functions *mpi_func;

// optimized version of persistent ops
struct mpiopt_functions {
  llvm::Function *mpi_wait = nullptr;
  llvm::Function *mpi_waitall = nullptr;
  llvm::Function *mpi_waitany = nullptr;
  llvm::Function *mpi_waitsome = nullptr;
  llvm::Function *mpi_test = nullptr;
  llvm::Function *mpi_testall = nullptr;
  llvm::Function *mpi_testany = nullptr;
  llvm::Function *mpi_testsome = nullptr;
  llvm::Function *mpi_start = nullptr;
  llvm::Function *mpi_startall = nullptr;
  llvm::Function *mpi_send_init = nullptr;
  llvm::Function *mpi_send_init_info = nullptr;
  llvm::Function *mpi_recv_init = nullptr;
  llvm::Function *mpi_recv_init_info = nullptr;
  llvm::Function *mpi_request_free = nullptr;
  llvm::Function *init = nullptr;
  llvm::Function *finalize = nullptr;
};

struct mpi_functions {
  llvm::Function *mpi_init = nullptr;
  llvm::Function *mpi_init_thread = nullptr;
  llvm::Function *mpi_finalize = nullptr;
  llvm::Function *mpi_comm_rank = nullptr;
  llvm::Function *mpi_comm_size = nullptr;

  llvm::Function *mpi_send = nullptr;
  llvm::Function *mpi_Bsend = nullptr;
  llvm::Function *mpi_Ssend = nullptr;
  llvm::Function *mpi_Rsend = nullptr;
  llvm::Function *mpi_Isend = nullptr;
  llvm::Function *mpi_Ibsend = nullptr;
  llvm::Function *mpi_Issend = nullptr;
  llvm::Function *mpi_Irsend = nullptr;

  llvm::Function *mpi_Sendrecv = nullptr;

  llvm::Function *mpi_recv = nullptr;
  llvm::Function *mpi_Irecv = nullptr;

  llvm::Function *mpi_test = nullptr;
  llvm::Function *mpi_testall = nullptr;
  llvm::Function *mpi_testany = nullptr;
  llvm::Function *mpi_testsome = nullptr;
  llvm::Function *mpi_wait = nullptr;
  llvm::Function *mpi_waitall = nullptr;
  llvm::Function *mpi_waitany = nullptr;
  llvm::Function *mpi_waitsome = nullptr;
  llvm::Function *mpi_buffer_detach = nullptr;

  llvm::Function *mpi_barrier = nullptr;
  llvm::Function *mpi_allreduce = nullptr;
  llvm::Function *mpi_Ibarrier = nullptr;
  llvm::Function *mpi_Iallreduce = nullptr;

  llvm::Function *mpi_wtime = nullptr;

  llvm::Function *mpi_start = nullptr;
  llvm::Function *mpi_startall = nullptr;
  llvm::Function *mpi_send_init = nullptr;
  llvm::Function *mpi_recv_init = nullptr;
  llvm::Function *mpi_request_free = nullptr;

  llvm::Function *mpi_info_create = nullptr;
  llvm::Function *mpi_info_set = nullptr;
  llvm::Function *mpi_info_free = nullptr;

  struct mpiopt_functions optimized;

  std::vector<llvm::CallBase *> send_calls; // all calls that send MPI messages
  std::vector<llvm::CallBase *> recv_calls; // all calls that recv mpi messages
};

struct mpi_functions *get_used_mpi_functions(llvm::Module &M);

bool is_mpi_used(struct mpi_functions *mpi_func);

bool is_mpi_call(llvm::CallBase *call);

bool is_mpi_function(llvm::Function *f);

bool is_send_function(llvm::Function *f);

bool is_recv_function(llvm::Function *f);

llvm::Value *get_tag_value(llvm::CallBase *mpi_call, bool is_send);
llvm::Value *get_src_value(llvm::CallBase *mpi_call, bool is_send);
llvm::Value *get_comm_value(llvm::CallBase *mpi_call, bool is_send);

#endif /* MACH_MPI_FUNCTIONS_H_ */
