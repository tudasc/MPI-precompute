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
#include "mpiopt_functions.h"
#include "precompute_backend_funcs.h"
#include "replacement.h"

#include "llvm/Transforms/IPO/ModuleInliner.h"

#include <precalculation.h>

using namespace llvm;

RequiredAnalysisResults *analysis_results;

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

llvm::CallBase *get_mpi_init_call(llvm::Module &M,
                                  llvm::Function *entry_point) {
  // search for MPI_init or Init Thread as precalc may only take place after
  // that
  CallBase *call_to_init = nullptr;
  if (get_mpi_functions(M)->mpi_init != nullptr) {
    for (auto *u : get_mpi_functions(M)->mpi_init->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        assert(call_to_init == nullptr && "MPI_Init is only allowed once");
        call_to_init = call;
      }
    }
  }
  if (get_mpi_functions(M)->mpi_init_thread != nullptr) {
    for (auto *u : get_mpi_functions(M)->mpi_init_thread->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        assert(call_to_init == nullptr && "MPI_Init is only allowed once");
        call_to_init = call;
      }
    }
  }

  assert(call_to_init != nullptr && "Did Not Found MPI_Init_Call");

  assert(call_to_init->getFunction() == entry_point &&
         "MPI_Init is not in main");
  return call_to_init;
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

    add_mpi_info_functions(M);

    // as this pass is used at LTO it sees the whole program so if no MPI is
    // used: nothing to do
    if (!is_mpi_initialized()) {
      // nothing to do for non mpi applications
      return PreservedAnalyses::all();
    }

    analysis_results = new RequiredAnalysisResults(AM, M);

    auto num_undef = get_num_undefs(M);

    // FrontendPluginData::create_instance(M);

    // collect all Persistent Comm Operations
    // std::vector<std::shared_ptr<PersistentMPIInitCall>> send_init_list;
    // std::vector<std::shared_ptr<PersistentMPIInitCall>> recv_init_list;
    std::vector<CallBase *> combined_init_list;

    if (get_mpi_functions(M)->mpi_send_init) {
      for (auto *u : get_mpi_functions(M)->mpi_send_init->users()) {
        if (auto *call = dyn_cast<CallBase>(u)) {
          if (call->getCalledFunction() ==
              get_mpi_functions(M)->mpi_send_init) {
            // not that I think anyone will pass a ptr to MPI func into another
            // func, but better save than sorry
            // send_init_list.push_back(
            //    PersistentMPIInitCall::get_PersistentMPIInitCall(call));
            combined_init_list.push_back(call);
          }
        }
      }
    }
    if (get_mpi_functions(M)->mpi_recv_init) {
      for (auto *u : get_mpi_functions(M)->mpi_recv_init->users()) {
        if (auto *call = dyn_cast<CallBase>(u)) {
          if (call->getCalledFunction() ==
              get_mpi_functions(M)->mpi_recv_init) {
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

      std::vector<Instruction *> init_calls; // for type conversion, as we need
                                             // to pass it as Instruction
      // collect values to be precomputed
      std::vector<Value *> to_precompute;
      for (auto *call : combined_init_list) {
        bool is_send = is_send_function(call->getCalledFunction());
        to_precompute.push_back(get_tag_value(call, is_send));
        to_precompute.push_back(get_src_value(call, is_send));
        init_calls.push_back(call);
      }

      auto init_call = get_mpi_init_call(M, main_func);

      auto precalcuation = std::make_shared<PrecomputeInsertion>(
          M, std::make_shared<PrecalculationAnalysis>(
                 M, main_func, to_precompute, init_calls));

      replace_MPI_with_precompute(precalcuation, get_mpi_functions(M),
                                  combined_init_list);

      add_call_to_precalculation_to_main(M, init_call, main_func,
                                         precalcuation);
      precalcuation->clean_precompute();

      remove_noinline_from_module(M);

      for (auto c : combined_init_list) {
        if (c->getCalledFunction() == get_mpi_functions(M)->mpi_recv_init) {
          replace_init_call(c, get_mpiopt_functions(M)->mpi_recv_init_info);
        } else if (c->getCalledFunction() ==
                   get_mpi_functions(M)->mpi_send_init) {
          replace_init_call(c, get_mpiopt_functions(M)->mpi_send_init_info);
        }
      }

      replace_request_handling_calls(M);
      add_init(M);
      add_finalize(M);
    }

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

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "mpi-matching", "1.0.0",
          [](PassBuilder &PB) {
            PB.registerOptimizerEarlyEPCallback([&](ModulePassManager &MPM,
                                                    OptimizationLevel Level,
                                                    ThinOrFullLTOPhase Phase) {
              MPM.addPass(MPICompilerAssistanceMatchingPass());
            });
          }};
}