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

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/IR/Verifier.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetLibraryInfo.h"

#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "precalculation.h"

#include <cassert>
#include <vector>

#include "analysis_results.h"
#include "debug.h"
#include "implementation_specific.h"
#include "mpi_functions.h"
#include "precompute_backend_funcs.h"
#include "replacement.h"

#include "llvm/Transforms/IPO/ModuleInliner.h"

using namespace llvm;

RequiredAnalysisResults *analysis_results;

struct mpi_functions *mpi_func;
ImplementationSpecifics *mpi_implementation_specifics;

// removes attribute noinline from every func
// we previously set it to make analysis easier
void remove_noinline_from_module(llvm::Module &M) {
  for (auto &F : M) {
    if (F.hasFnAttribute(llvm::Attribute::NoInline) and
        not F.hasFnAttribute(llvm::Attribute::OptimizeNone)) {
      F.removeFnAttr(llvm::Attribute::NoInline);
    }
  }
}

void run_optimization_passes(llvm::Module &M, ModuleAnalysisManager &AM) {
  errs() << "Run inliner Pass\n";

  auto inliner = llvm::ModuleInlinerPass();
  inliner.run(M, AM);

  // M.dump();
}

namespace {
struct MPICompilerAssistanceMatchingPass
    : public PassInfoMixin<MPICompilerAssistanceMatchingPass> {

  // register that we require this analysis

  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addRequired<ModuleSummaryIndexWrapperPass>();
    AU.addRequiredTransitive<AAResultsWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
  }

  StringRef getPassName() const { return "mpi-matching"; }

  // Pass starts here
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {

    Debug(errs() << "Before Modification:\n"; M.dump();
          errs() << "END MODULE\n";);

    ImplementationSpecifics::create_instance(M);
    PrecomputeFunctions::create_instance(M);

    mpi_func = get_used_mpi_functions(M);

    // as this pass is used at LTO it sees the whole program so if no MPI is
    // used: nothing to do
    if (!is_mpi_used(mpi_func)) {
      // nothing to do for non mpi applicatiopns
      delete mpi_func;
      return PreservedAnalyses::all();
    }

    analysis_results = new RequiredAnalysisResults(AM, M);

    auto num_undef = get_num_undefs(M);

    // FrontendPluginData::create_instance(M);

    // collect all Persistent Comm Operations
    // std::vector<std::shared_ptr<PersistentMPIInitCall>> send_init_list;
    // std::vector<std::shared_ptr<PersistentMPIInitCall>> recv_init_list;
    std::vector<CallBase *> combined_init_list;

    if (mpi_func->mpi_send_init) {
      for (auto *u : mpi_func->mpi_send_init->users()) {
        if (auto *call = dyn_cast<CallBase>(u)) {
          if (call->getCalledFunction() == mpi_func->mpi_send_init) {
            // not that I think anyone will pass a ptr to MPI func into another
            // func, but better save than sorry
            // send_init_list.push_back(
            //    PersistentMPIInitCall::get_PersistentMPIInitCall(call));
            combined_init_list.push_back(call);
          }
        }
      }
    }
    if (mpi_func->mpi_recv_init) {
      for (auto *u : mpi_func->mpi_recv_init->users()) {
        if (auto *call = dyn_cast<CallBase>(u)) {
          if (call->getCalledFunction() == mpi_func->mpi_recv_init) {
            // recv_init_list.push_back(
            //     PersistentMPIInitCall::get_PersistentMPIInitCall(call));
            combined_init_list.push_back(call);
          }
        }
      }
    }

    auto *main_func = M.getFunction("main");
    assert(main_func);

    bool replacement = !combined_init_list.empty();
    // otherwise nothing should be done
    if (replacement) {
      auto precalcuation =
          std::make_shared<PrecalculationAnalysis>(M, main_func);
      precalcuation->add_precalculations(combined_init_list);

      remove_noinline_from_module(M);

      for (auto c : combined_init_list) {
        if (c->getCalledFunction() == mpi_func->mpi_recv_init) {
          replace_init_call(c, mpi_func->optimized.mpi_recv_init_info);
        } else if (c->getCalledFunction() == mpi_func->mpi_send_init) {
          replace_init_call(c, mpi_func->optimized.mpi_send_init_info);
        }
      }

      replace_request_handling_calls(M);
      add_init(M);
      add_finalize(M);
    }

    delete mpi_func;
    ImplementationSpecifics::delete_instance();
    PrecomputeFunctions::delete_instance();
    // FrontendPluginData::delete_instance();

    delete analysis_results;

    Debug(errs() << "After Modification:\n"; M.dump();
          errs() << "END MODULE\n";);

#ifndef NDEBUG
    auto has_error = verifyModule(M, &errs(), nullptr);
    assert(!has_error);
    // at most: every undef value can be duplicated
    // TODO re-enable assertions for no openmp programs
    // assert(get_num_undefs(M) <= num_undef * 2);
    // but this is probably insecure (e.g. if undef is used to calculate the
    // tag)// so we go with the stricter assertion that our pass should not use
    // more undef values
    // assert(get_num_undefs(M) <= num_undef);
    // some undefs are actually duplicated in our test programm (some vector
    // elems are undef)
#endif

    errs() << "Successfully executed the pass\n\n";

    run_optimization_passes(M, AM);

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
    PB.registerOptimizerEarlyEPCallback([&](ModulePassManager &MPM, auto) {
      MPM.addPass(MPICompilerAssistanceMatchingPass());
      return true;
    });
  };

  return {LLVM_PLUGIN_API_VERSION, "mpi-matching", "1.0.0", callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}