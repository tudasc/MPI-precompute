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
#ifndef MPI_ASSERTION_CHECKING_PRECOMPUTE_INSERTION_H
#define MPI_ASSERTION_CHECKING_PRECOMPUTE_INSERTION_H

#include "precalculation.h"
#include <llvm/IR/Module.h>
void replace_MPI_with_precompute(
    const std::shared_ptr<PrecalculationAnalysis> &precompute_analyis_result,
    struct mpi_functions *mpi_func,
    const std::vector<llvm::CallBase *> &init_calls);

void add_call_to_precalculation_to_main(
    llvm::Module &M, llvm::CallBase *call_to_init,
    llvm::Function *entry_function,
    const std::shared_ptr<PrecalculationAnalysis> &precompute_analyis_result);

#endif // MPI_ASSERTION_CHECKING_PRECOMPUTE_INSERTION_H
