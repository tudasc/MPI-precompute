#include "devirt_analysis.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/TypeMetadataUtils.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ModuleSummaryIndexYAML.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Errc.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/GlobPattern.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/FunctionAttrs.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/CallPromotionUtils.h"
#include "llvm/Transforms/Utils/Evaluator.h"
#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <string>

using namespace std;
using namespace llvm;

bool DevirtModule::mustBeUnreachableFunction(
    Function *const F, ModuleSummaryIndex *ExportSummary) {
  // First, learn unreachability by analyzing function IR.
  if (!F->isDeclaration()) {
    // A function must be unreachable if its entry block ends with an
    // 'unreachable'.
    return isa<UnreachableInst>(F->getEntryBlock().getTerminator());
  }
  // we removed this for our pass
  // Learn unreachability from ExportSummary if ExportSummary is present.
  return false;
}

bool DevirtModule::tryFindVirtualCallTargets(
    std::vector<VirtualCallTarget> &TargetsForSlot,
    const std::set<TypeMemberInfo> &TypeMemberInfos, uint64_t ByteOffset,
    ModuleSummaryIndex *ExportSummary) {
  for (const TypeMemberInfo &TM : TypeMemberInfos) {
    if (!TM.Bits->GV->isConstant())
      return false;

    // We cannot perform whole program devirtualization analysis on a vtable
    // with public LTO visibility.
    if (TM.Bits->GV->getVCallVisibility() ==
        GlobalObject::VCallVisibilityPublic)
      return false;

    Constant *Ptr = getPointerAtOffset(TM.Bits->GV->getInitializer(),
                                       TM.Offset + ByteOffset, M);
    if (!Ptr)
      return false;

    auto Fn = dyn_cast<Function>(Ptr->stripPointerCasts());
    if (!Fn)
      return false;

    /*
    if (FunctionsToSkip.match(Fn->getName()))
      return false;
    */
    // We can disregard __cxa_pure_virtual as a possible call target, as
    // calls to pure virtuals are UB.
    if (Fn->getName() == "__cxa_pure_virtual")
      continue;

    // We can disregard unreachable functions as possible call targets, as
    // unreachable functions shouldn't be called.
    if (mustBeUnreachableFunction(Fn, ExportSummary))
      continue;

    TargetsForSlot.push_back({Fn, &TM});
  }

  // Give up if we couldn't find any targets.
  return !TargetsForSlot.empty();
}

bool DevirtModule::run() {

  std::unique_ptr<ModuleSummaryIndex> Summary =
      std::make_unique<ModuleSummaryIndex>(/*HaveGVs=*/false);
  // TODO
  //  get this obj from llvm::ModuleSummaryIndexAnalysis

  Function *TypeTestFunc =
      M.getFunction(Intrinsic::getName(Intrinsic::type_test));
  Function *TypeCheckedLoadFunc =
      M.getFunction(Intrinsic::getName(Intrinsic::type_checked_load));
  Function *AssumeFunc = M.getFunction(Intrinsic::getName(Intrinsic::assume));

  // Rebuild type metadata into a map for easy lookup.
  std::vector<VTableBits> Bits;
  DenseMap<Metadata *, std::set<TypeMemberInfo>> TypeIdMap;
  buildTypeIdentifierMap(Bits, TypeIdMap);

  if (TypeTestFunc && AssumeFunc)
    scanTypeTestUsers(TypeTestFunc, TypeIdMap);

  if (TypeCheckedLoadFunc)
    scanTypeCheckedLoadUsers(TypeCheckedLoadFunc);

  // Collect information from summary about which calls to try to devirtualize.
  if (ExportSummary) {
    DenseMap<GlobalValue::GUID, TinyPtrVector<Metadata *>> MetadataByGUID;
    for (auto &P : TypeIdMap) {
      if (auto *TypeId = dyn_cast<MDString>(P.first))
        MetadataByGUID[GlobalValue::getGUID(TypeId->getString())].push_back(
            TypeId);
    }

    for (auto &P : *ExportSummary) {
      for (auto &S : P.second.SummaryList) {
        auto *FS = dyn_cast<FunctionSummary>(S.get());
        if (!FS)
          continue;
        // FIXME: Only add live functions.
        for (FunctionSummary::VFuncId VF : FS->type_test_assume_vcalls()) {
          for (Metadata *MD : MetadataByGUID[VF.GUID]) {
            CallSlots[{MD, VF.Offset}].CSInfo.addSummaryTypeTestAssumeUser(FS);
          }
        }
        for (FunctionSummary::VFuncId VF : FS->type_checked_load_vcalls()) {
          for (Metadata *MD : MetadataByGUID[VF.GUID]) {
            CallSlots[{MD, VF.Offset}].CSInfo.addSummaryTypeCheckedLoadUser(FS);
          }
        }
        for (const FunctionSummary::ConstVCall &VC :
             FS->type_test_assume_const_vcalls()) {
          for (Metadata *MD : MetadataByGUID[VC.VFunc.GUID]) {
            CallSlots[{MD, VC.VFunc.Offset}]
                .ConstCSInfo[VC.Args]
                .addSummaryTypeTestAssumeUser(FS);
          }
        }
        for (const FunctionSummary::ConstVCall &VC :
             FS->type_checked_load_const_vcalls()) {
          for (Metadata *MD : MetadataByGUID[VC.VFunc.GUID]) {
            CallSlots[{MD, VC.VFunc.Offset}]
                .ConstCSInfo[VC.Args]
                .addSummaryTypeCheckedLoadUser(FS);
          }
        }
      }
    }
  }

  for (auto &S : CallSlots) {
    std::vector<VirtualCallTarget> TargetsForSlot;
    WholeProgramDevirtResolution *Res = nullptr;
    const std::set<TypeMemberInfo> &TypeMemberInfos = TypeIdMap[S.first.TypeID];
    if (ExportSummary && isa<MDString>(S.first.TypeID) &&
        TypeMemberInfos.size())
      // For any type id used on a global's type metadata, create the type id
      // summary resolution regardless of whether we can devirtualize, so that
      // lower type tests knows the type id is not Unsat. If it was not used on
      // a global's type metadata, the TypeIdMap entry set will be empty, and
      // we don't want to create an entry (with the default Unknown type
      // resolution), which can prevent detection of the Unsat.
      Res = &ExportSummary
                 ->getOrInsertTypeIdSummary(
                     cast<MDString>(S.first.TypeID)->getString())
                 .WPDRes[S.first.ByteOffset];
    if (tryFindVirtualCallTargets(TargetsForSlot, TypeMemberInfos,
                                  S.first.ByteOffset, ExportSummary)) {
      errs() << "For calls:\n";
      VTableSlotInfo CSInfo;
      for (auto &cinfo : CSInfo.CSInfo.CallSites) {
        cinfo.CB.dump();
      }
      errs() << "possible targets:\n";
      for (auto slot : TargetsForSlot) {
        errs() << slot.Fn->getName() << "\n";
      }
    }
  }

  assert(false && "DEBUG ASSERTION");
}