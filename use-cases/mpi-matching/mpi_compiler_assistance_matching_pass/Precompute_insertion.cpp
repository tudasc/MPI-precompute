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
#include "Precompute_insertion.h"
#include "CompilerPassConstants.h"
#include "debug.h"
#include "implementation_specific.h"
#include "precalculation.h"
#include "precompute_backend_funcs.h"

#include "mpi_functions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

void replace_MPI_with_precompute(
    const std::shared_ptr<PrecalculationAnalysis> &precompute_analyis_result,
    const std::vector<llvm::CallBase *> &init_calls) {
  for (auto *old_call : init_calls) {
    const bool is_send = is_send_function(old_call->getCalledFunction());
    auto *old_tag = get_tag_value(old_call, is_send);
    auto *old_src = get_src_value(old_call, is_send);

    // TODO this map is currently not build correctly
    auto *precomputed_call =
        precompute_analyis_result->get_precomputed_value(old_call);

    auto *precomputed_tag =
        precompute_analyis_result->get_precomputed_value(old_tag);
    auto *precomputed_src =
        precompute_analyis_result->get_precomputed_value(old_src);

    assert(precomputed_tag ==
           get_tag_value(cast<CallBase>(precomputed_call), is_send));
    assert(precomputed_src ==
           get_src_value(cast<CallBase>(precomputed_call), is_send));

    assert(precomputed_call != nullptr && precomputed_tag != nullptr &&
           precomputed_src != nullptr);
    assert(isa<CallBase>(precomputed_call));

    IRBuilder<> builder = IRBuilder<>(cast<Instruction>(precomputed_call));

    int precompute_envelope_dest;
    int precompute_envelope_tag;
    if (old_call->getCalledFunction() ==
        precompute_analyis_result->mpi_func->mpi_send_init) {
      precompute_envelope_dest = SEND_ENVELOPE_DEST;
      precompute_envelope_tag = SEND_ENVELOPE_TAG;
    } else {
      assert(old_call->getCalledFunction() ==
             precompute_analyis_result->mpi_func->mpi_recv_init);
      precompute_envelope_dest = RECV_ENVELOPE_DEST;
      precompute_envelope_tag = RECV_ENVELOPE_TAG;
    }

    builder.CreateCall(
        PrecomputeFunctions::get_instance()->register_precomputed_value,
        {builder.getInt32(precompute_envelope_dest), precomputed_src});

    builder.CreateCall(
        PrecomputeFunctions::get_instance()->register_precomputed_value,
        {builder.getInt32(precompute_envelope_tag), precomputed_tag});
  }
}

void add_call_to_precalculation_to_main(
    llvm::Module &M, llvm::CallBase *call_to_init,
    llvm::Function *entry_function,
    const std::shared_ptr<PrecalculationAnalysis> &precompute_analyis_result) {

  // insert after init
  IRBuilder<> builder(call_to_init->getNextNode());

  // forward args of main
  std::vector<Value *> args;
  for (auto &arg : entry_function->args()) {
    args.push_back(&arg);
  }
  builder.CreateCall(precompute_analyis_result->get_precompute_phase_main(),
                     args);
}
