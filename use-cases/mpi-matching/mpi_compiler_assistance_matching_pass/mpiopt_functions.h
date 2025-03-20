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
#ifndef MACH_MPIOPT_FUNCTIONS_H_
#define MACH_MPIOPT_FUNCTIONS_H_

#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"

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

struct mpiopt_functions *get_mpiopt_functions(llvm::Module &M);

void add_mpi_info_functions(llvm::Module &M);

#endif /* MACH_MPIOPT_FUNCTIONS_H_ */
