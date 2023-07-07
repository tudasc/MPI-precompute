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

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/IR/Verifier.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetLibraryInfo.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include <assert.h>
// #include <mpi.h>
#include <cstring>
#include <utility>
#include <vector>

#include "analysis_results.h"
#include "conflict_detection.h"
#include "debug.h"
#include "frontend_plugin_data.h"
#include "implementation_specific.h"
#include "mpi_functions.h"
#include "replacement.h"

using namespace llvm;

// declare dso_local i32 @MPI_Recv(i8*, i32, i32, i32, i32, i32,
// %struct.MPI_Status*) #1

RequiredAnalysisResults *analysis_results;

struct mpi_functions *mpi_func;
ImplementationSpecifics *mpi_implementation_specifics;

namespace {
struct MPICompilerAssistanceMatchingPass
    : public PassInfoMixin<MPICompilerAssistanceMatchingPass> {

  // register that we require this analysis

  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addRequiredTransitive<AAResultsWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
  }

  StringRef getPassName() const { return "MPI Assertion Analysis"; }

  // Pass starts here
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {

    // Debug(M.dump(););

    ImplementationSpecifics::create_instance(M);

    mpi_func = get_used_mpi_functions(M);
    // TODO is_mpi_used only checks for MPI init, but we want to use this on
    // apps with muliple translation units
    /*if (!is_mpi_used(mpi_func)) {
      // nothing to do for non mpi applicatiopns
      delete mpi_func;
      return false;
    }*/

    analysis_results = new RequiredAnalysisResults(AM, M);

    FrontendPluginData::create_instance(M);

    // collect all Persistent Comm Operations
    std::vector<llvm::CallBase *> send_init_list;
    std::vector<llvm::CallBase *> recv_init_list;

    if (mpi_func->mpi_send_init) {
      for (auto *u : mpi_func->mpi_send_init->users()) {
        if (auto *call = dyn_cast<CallBase>(u)) {
          if (call->getCalledFunction() == mpi_func->mpi_send_init) {
            // not that I think anyone will pass a ptr to MPI func into another
            // func, but better save than sorry
            send_init_list.push_back(call);
          }
        }
      }
    }
    if (mpi_func->mpi_recv_init) {
      for (auto *u : mpi_func->mpi_recv_init->users()) {
        if (auto *call = dyn_cast<CallBase>(u)) {
          if (call->getCalledFunction() == mpi_func->mpi_recv_init) {
            recv_init_list.push_back(call);
          }
        }
      }
    }

    // remove any calls, where conflicts are possible
    send_init_list.erase(std::remove_if(send_init_list.begin(),
                                        send_init_list.end(),
                                        check_mpi_send_conflicts),
                         send_init_list.end());

    recv_init_list.erase(std::remove_if(recv_init_list.begin(),
                                        recv_init_list.end(),
                                        check_mpi_recv_conflicts),
                         recv_init_list.end());

    bool replacement = !send_init_list.empty() && !recv_init_list.empty();
    // otherwise nothing should be done
    if (replacement) {

      errs() << "Replace " << send_init_list.size() << " send Operations \nand "
             << recv_init_list.size() << " receive Operations\n";

      replace_communication_calls(M, send_init_list, recv_init_list);
    }

    // Beware: with operator | the functions will be executed with || they wont
    replacement = replacement | add_init(M);
    replacement = replacement | add_finalize(M);

    errs() << "Successfully executed the pass\n\n";
    delete mpi_func;
    ImplementationSpecifics::delete_instance();
    FrontendPluginData::delete_instance();

    delete analysis_results;

#ifndef NDEBUG
    auto has_error = verifyModule(M, &errs(), nullptr);
    assert(!has_error);
#endif

    if (replacement) {
      return PreservedAnalyses::none();
    } else {
      return PreservedAnalyses::all();
    }
  }
};
// class MSGOrderRelaxCheckerPass
} // namespace

PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    PB.registerPipelineEarlySimplificationEPCallback(
        [&](ModulePassManager &MPM, auto) {
          MPM.addPass(MPICompilerAssistanceMatchingPass());
          return true;
        });
  };

  return {LLVM_PLUGIN_API_VERSION, "mpi-matching", "1.0.0", callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}