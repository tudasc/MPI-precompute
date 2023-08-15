#include "devirt_analysis.h"
#include "analysis_results.h"
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

CallSiteInfo &VTableSlotInfo::findCallSiteInfo(CallBase &CB) {
  std::vector<uint64_t> Args;
  auto *CBType = dyn_cast<IntegerType>(CB.getType());
  if (!CBType || CBType->getBitWidth() > 64 || CB.arg_empty())
    return CSInfo;
  for (auto &&Arg : drop_begin(CB.args())) {
    auto *CI = dyn_cast<ConstantInt>(Arg);
    if (!CI || CI->getBitWidth() > 64)
      return CSInfo;
    Args.push_back(CI->getZExtValue());
  }
  return ConstCSInfo[Args];
}

void VTableSlotInfo::addCallSite(Value *VTable, CallBase &CB,
                                 unsigned *NumUnsafeUses) {
  auto &CSI = findCallSiteInfo(CB);
  CSI.AllCallSitesDevirted = false;
  CSI.CallSites.push_back({VTable, CB, NumUnsafeUses});
}

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

void DevirtModule::buildTypeIdentifierMap(
    std::vector<VTableBits> &Bits,
    DenseMap<Metadata *, std::set<TypeMemberInfo>> &TypeIdMap) {
  DenseMap<GlobalVariable *, VTableBits *> GVToBits;
  Bits.reserve(M.getGlobalList().size());
  SmallVector<MDNode *, 2> Types;
  for (GlobalVariable &GV : M.globals()) {
    Types.clear();
    GV.getMetadata(LLVMContext::MD_type, Types);
    if (GV.isDeclaration() || Types.empty())
      continue;

    VTableBits *&BitsPtr = GVToBits[&GV];
    if (!BitsPtr) {
      Bits.emplace_back();
      Bits.back().GV = &GV;
      Bits.back().ObjectSize =
          M.getDataLayout().getTypeAllocSize(GV.getInitializer()->getType());
      BitsPtr = &Bits.back();
    }

    for (MDNode *Type : Types) {
      auto TypeID = Type->getOperand(1).get();

      uint64_t Offset =
          cast<ConstantInt>(
              cast<ConstantAsMetadata>(Type->getOperand(0))->getValue())
              ->getZExtValue();

      TypeIdMap[TypeID].insert({BitsPtr, Offset});
    }
  }
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

  Function *TypeTestFunc =
      M.getFunction(Intrinsic::getName(Intrinsic::type_test));
  Function *TypeCheckedLoadFunc =
      M.getFunction(Intrinsic::getName(Intrinsic::type_checked_load));
  Function *AssumeFunc = M.getFunction(Intrinsic::getName(Intrinsic::assume));

  // TODO what is actually the difference between those??
  if (not TypeTestFunc)
    TypeTestFunc =
        M.getFunction(Intrinsic::getName(Intrinsic::public_type_test));

  if (TypeTestFunc)
    TypeTestFunc->dump();
  if (TypeCheckedLoadFunc)
    TypeCheckedLoadFunc->dump();
  if (AssumeFunc)
    AssumeFunc->dump();

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
  // For each (type, offset) pair:
  bool DidVirtualConstProp = false;
  std::map<std::string, Function *> DevirtTargets;
  for (auto &S : CallSlots) {
    errs() << "For calls:\n";
    VTableSlotInfo &CSInfo = S.second;
    for (auto &cinfo : CSInfo.CSInfo.CallSites) {
      cinfo.CB.dump();
    }
    // Search each of the members of the type identifier for the virtual
    // function implementation at offset S.first.ByteOffset, and add to
    // TargetsForSlot.
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
      VTableSlotInfo &CSInfo = S.second;
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

void DevirtModule::scanTypeTestUsers(
    Function *TypeTestFunc,
    DenseMap<Metadata *, std::set<TypeMemberInfo>> &TypeIdMap) {

  errs() << "scanTypeTestUsers\n";
  // Find all virtual calls via a virtual table pointer %p under an assumption
  // of the form llvm.assume(llvm.type.test(%p, %md)). This indicates that %p
  // points to a member of the type identifier %md. Group calls by (type ID,
  // offset) pair (effectively the identity of the virtual function) and store
  // to CallSlots.
  for (Use &U : llvm::make_early_inc_range(TypeTestFunc->uses())) {
    auto *CI = dyn_cast<CallInst>(U.getUser());
    if (!CI)
      continue;
    CI->dump();

    // Search for virtual calls based on %p and add them to DevirtCalls.
    SmallVector<DevirtCallSite, 1> DevirtCalls;
    SmallVector<CallInst *, 1> Assumes;
    auto DT = analysis_results->getDomTree(*CI->getFunction());
    findDevirtualizableCallsForTypeTest(DevirtCalls, Assumes, CI, *DT);

    Metadata *TypeId =
        cast<MetadataAsValue>(CI->getArgOperand(1))->getMetadata();
    // If we found any, add them to CallSlots.
    if (!Assumes.empty()) {
      Value *Ptr = CI->getArgOperand(0)->stripPointerCasts();
      for (DevirtCallSite Call : DevirtCalls)
        CallSlots[{TypeId, Call.Offset}].addCallSite(Ptr, Call.CB, nullptr);
    }

    auto RemoveTypeTestAssumes = [&]() {
      // We no longer need the assumes or the type test.
      for (auto *Assume : Assumes)
        Assume->eraseFromParent();
      // We can't use RecursivelyDeleteTriviallyDeadInstructions here because we
      // may use the vtable argument later.
      if (CI->use_empty())
        CI->eraseFromParent();
    };

    // At this point we could remove all type test assume sequences, as they
    // were originally inserted for WPD. However, we can keep these in the
    // code stream for later analysis (e.g. to help drive more efficient ICP
    // sequences). They will eventually be removed by a second LowerTypeTests
    // invocation that cleans them up. In order to do this correctly, the first
    // LowerTypeTests invocation needs to know that they have "Unknown" type
    // test resolution, so that they aren't treated as Unsat and lowered to
    // False, which will break any uses on assumes. Below we remove any type
    // test assumes that will not be treated as Unknown by LTT.

    // The type test assumes will be treated by LTT as Unsat if the type id is
    // not used on a global (in which case it has no entry in the TypeIdMap).
    if (!TypeIdMap.count(TypeId))
      RemoveTypeTestAssumes();
  }
}

void DevirtModule::scanTypeCheckedLoadUsers(Function *TypeCheckedLoadFunc) {
  Function *TypeTestFunc = Intrinsic::getDeclaration(&M, Intrinsic::type_test);

  errs() << "scanTypeCheckedLoadUsers\n";

  for (Use &U : llvm::make_early_inc_range(TypeCheckedLoadFunc->uses())) {
    auto *CI = dyn_cast<CallInst>(U.getUser());
    if (!CI)
      continue;

    Value *Ptr = CI->getArgOperand(0);
    Value *Offset = CI->getArgOperand(1);
    Value *TypeIdValue = CI->getArgOperand(2);
    Metadata *TypeId = cast<MetadataAsValue>(TypeIdValue)->getMetadata();

    SmallVector<DevirtCallSite, 1> DevirtCalls;
    SmallVector<Instruction *, 1> LoadedPtrs;
    SmallVector<Instruction *, 1> Preds;
    bool HasNonCallUses = false;
    auto DT = analysis_results->getDomTree(*CI->getFunction());
    findDevirtualizableCallsForTypeCheckedLoad(DevirtCalls, LoadedPtrs, Preds,
                                               HasNonCallUses, CI, *DT);

    // Start by generating "pessimistic" code that explicitly loads the function
    // pointer from the vtable and performs the type check. If possible, we will
    // eliminate the load and the type check later.

    // If possible, only generate the load at the point where it is used.
    // This helps avoid unnecessary spills.
    IRBuilder<> LoadB(
        (LoadedPtrs.size() == 1 && !HasNonCallUses) ? LoadedPtrs[0] : CI);
    Value *GEP = LoadB.CreateGEP(Int8Ty, Ptr, Offset);
    Value *GEPPtr = LoadB.CreateBitCast(GEP, PointerType::getUnqual(Int8PtrTy));
    Value *LoadedValue = LoadB.CreateLoad(Int8PtrTy, GEPPtr);

    for (Instruction *LoadedPtr : LoadedPtrs) {
      LoadedPtr->replaceAllUsesWith(LoadedValue);
      LoadedPtr->eraseFromParent();
    }

    // Likewise for the type test.
    IRBuilder<> CallB((Preds.size() == 1 && !HasNonCallUses) ? Preds[0] : CI);
    CallInst *TypeTestCall = CallB.CreateCall(TypeTestFunc, {Ptr, TypeIdValue});

    for (Instruction *Pred : Preds) {
      Pred->replaceAllUsesWith(TypeTestCall);
      Pred->eraseFromParent();
    }

    // We have already erased any extractvalue instructions that refer to the
    // intrinsic call, but the intrinsic may have other non-extractvalue uses
    // (although this is unlikely). In that case, explicitly build a pair and
    // RAUW it.
    if (!CI->use_empty()) {
      Value *Pair = PoisonValue::get(CI->getType());
      IRBuilder<> B(CI);
      Pair = B.CreateInsertValue(Pair, LoadedValue, {0});
      Pair = B.CreateInsertValue(Pair, TypeTestCall, {1});
      CI->replaceAllUsesWith(Pair);
    }

    // The number of unsafe uses is initially the number of uses.
    auto &NumUnsafeUses = NumUnsafeUsesForTypeTest[TypeTestCall];
    NumUnsafeUses = DevirtCalls.size();

    // If the function pointer has a non-call user, we cannot eliminate the type
    // check, as one of those users may eventually call the pointer. Increment
    // the unsafe use count to make sure it cannot reach zero.
    if (HasNonCallUses)
      ++NumUnsafeUses;
    for (DevirtCallSite Call : DevirtCalls) {
      CallSlots[{TypeId, Call.Offset}].addCallSite(Ptr, Call.CB,
                                                   &NumUnsafeUses);
    }

    CI->eraseFromParent();
  }
}