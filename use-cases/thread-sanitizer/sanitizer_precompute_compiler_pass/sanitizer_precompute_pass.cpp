
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
#include "precompute_backend_funcs.h"

#include "llvm/Transforms/IPO/ModuleInliner.h"

#include <precalculation_impl.h>

using namespace llvm;

RequiredAnalysisResults *analysis_results;

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
struct SanitizerPrecomputePass : public PassInfoMixin<SanitizerPrecomputePass> {

  // register that we require this analysis

  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addRequired<ModuleSummaryIndexWrapperPass>();
    AU.addRequiredTransitive<AAResultsWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
  }

  StringRef getPassName() const { return "sanitizer-precompute"; }

  // Pass starts here
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {

    auto MSI = &AM.getResult<ModuleSummaryIndexAnalysis>(M);
    errs() << "got analysis result\n";

    Debug(errs() << "Before Modification:\n"; M.dump();
          errs() << "END MODULE\n";);
    auto has_error2 = verifyModule(M, &errs(), nullptr);
    assert(!has_error2);

    PrecomputeFunctions::create_instance(M);
    errs() << "created PrecomputeFunctions\n";
    analysis_results = new RequiredAnalysisResults(AM, M);
    errs() << "created Required Analysis\n";

    auto num_undef = get_num_undefs(M);

    auto *main_func = M.getFunction("main");
    assert(main_func);

    /*
          auto precalcuation = PrecalculationAnalysisFactroy(
              M, main_func, to_precompute, init_calls);
          precalcuation->generate_slice();


          precalcuation->clean_precompute();
    */
    remove_noinline_from_module(M);

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

    return PreservedAnalyses::none();
  }
};

} // namespace

PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    PB.registerOptimizerEarlyEPCallback([&](ModulePassManager &MPM, auto) {
      MPM.addPass(SanitizerPrecomputePass());
      return true;
    });
  };

  return {LLVM_PLUGIN_API_VERSION, "sanitizer-precompute", "1.0.0", callback};
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}