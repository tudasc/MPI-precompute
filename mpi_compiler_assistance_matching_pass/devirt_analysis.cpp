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

// TODO correct source
// most code from llvms' WholeProgramDevirtPass

using namespace std;
using namespace llvm;

using namespace llvm;
using namespace std;

// A bit vector that keeps track of which bits are used. We use this to
// pack constant values compactly before and after each virtual table.
struct AccumBitVector {
  std::vector<uint8_t> Bytes;

  // Bits in BytesUsed[I] are 1 if matching bit in Bytes[I] is used, 0 if not.
  std::vector<uint8_t> BytesUsed;

  std::pair<uint8_t *, uint8_t *> getPtrToData(uint64_t Pos, uint8_t Size) {
    if (Bytes.size() < Pos + Size) {
      Bytes.resize(Pos + Size);
      BytesUsed.resize(Pos + Size);
    }
    return std::make_pair(Bytes.data() + Pos, BytesUsed.data() + Pos);
  }

  // Set little-endian value Val with size Size at bit position Pos,
  // and mark bytes as used.
  void setLE(uint64_t Pos, uint64_t Val, uint8_t Size) {
    assert(Pos % 8 == 0);
    auto DataUsed = getPtrToData(Pos / 8, Size);
    for (unsigned I = 0; I != Size; ++I) {
      DataUsed.first[I] = Val >> (I * 8);
      assert(!DataUsed.second[I]);
      DataUsed.second[I] = 0xff;
    }
  }

  // Set big-endian value Val with size Size at bit position Pos,
  // and mark bytes as used.
  void setBE(uint64_t Pos, uint64_t Val, uint8_t Size) {
    assert(Pos % 8 == 0);
    auto DataUsed = getPtrToData(Pos / 8, Size);
    for (unsigned I = 0; I != Size; ++I) {
      DataUsed.first[Size - I - 1] = Val >> (I * 8);
      assert(!DataUsed.second[Size - I - 1]);
      DataUsed.second[Size - I - 1] = 0xff;
    }
  }

  // Set bit at bit position Pos to b and mark bit as used.
  void setBit(uint64_t Pos, bool b) {
    auto DataUsed = getPtrToData(Pos / 8, 1);
    if (b)
      *DataUsed.first |= 1 << (Pos % 8);
    assert(!(*DataUsed.second & (1 << Pos % 8)));
    *DataUsed.second |= 1 << (Pos % 8);
  }
};

// The bits that will be stored before and after a particular vtable.
struct VTableBits {
  // The vtable global.
  GlobalVariable *GV;

  // Cache of the vtable's size in bytes.
  uint64_t ObjectSize = 0;

  // The bit vector that will be laid out before the vtable. Note that these
  // bytes are stored in reverse order until the globals are rebuilt. This means
  // that any values in the array must be stored using the opposite endianness
  // from the target.
  AccumBitVector Before;

  // The bit vector that will be laid out after the vtable.
  AccumBitVector After;
};

// Information about a member of a particular type identifier.
struct TypeMemberInfo {
  // The VTableBits for the vtable.
  VTableBits *Bits;

  // The offset in bytes from the start of the vtable (i.e. the address point).
  uint64_t Offset;

  bool operator<(const TypeMemberInfo &other) const {
    return Bits < other.Bits || (Bits == other.Bits && Offset < other.Offset);
  }
};

// A virtual call target, i.e. an entry in a particular vtable.
struct VirtualCallTarget {
  VirtualCallTarget(Function *Fn, const TypeMemberInfo *TM);

  // For testing only.
  VirtualCallTarget(const TypeMemberInfo *TM, bool IsBigEndian)
      : Fn(nullptr), TM(TM), IsBigEndian(IsBigEndian), WasDevirt(false) {}

  // The function stored in the vtable.
  Function *Fn;

  // A pointer to the type identifier member through which the pointer to Fn is
  // accessed.
  const TypeMemberInfo *TM;

  // When doing virtual constant propagation, this stores the return value for
  // the function when passed the currently considered argument list.
  uint64_t RetVal;

  // Whether the target is big endian.
  bool IsBigEndian;

  // Whether at least one call site to the target was devirtualized.
  bool WasDevirt;

  // The minimum byte offset before the address point. This covers the bytes in
  // the vtable object before the address point (e.g. RTTI, access-to-top,
  // vtables for other base classes) and is equal to the offset from the start
  // of the vtable object to the address point.
  uint64_t minBeforeBytes() const { return TM->Offset; }

  // The minimum byte offset after the address point. This covers the bytes in
  // the vtable object after the address point (e.g. the vtable for the current
  // class and any later base classes) and is equal to the size of the vtable
  // object minus the offset from the start of the vtable object to the address
  // point.
  uint64_t minAfterBytes() const { return TM->Bits->ObjectSize - TM->Offset; }

  // The number of bytes allocated (for the vtable plus the byte array) before
  // the address point.
  uint64_t allocatedBeforeBytes() const {
    return minBeforeBytes() + TM->Bits->Before.Bytes.size();
  }

  // The number of bytes allocated (for the vtable plus the byte array) after
  // the address point.
  uint64_t allocatedAfterBytes() const {
    return minAfterBytes() + TM->Bits->After.Bytes.size();
  }

  // Set the bit at position Pos before the address point to RetVal.
  void setBeforeBit(uint64_t Pos) {
    assert(Pos >= 8 * minBeforeBytes());
    TM->Bits->Before.setBit(Pos - 8 * minBeforeBytes(), RetVal);
  }

  // Set the bit at position Pos after the address point to RetVal.
  void setAfterBit(uint64_t Pos) {
    assert(Pos >= 8 * minAfterBytes());
    TM->Bits->After.setBit(Pos - 8 * minAfterBytes(), RetVal);
  }

  // Set the bytes at position Pos before the address point to RetVal.
  // Because the bytes in Before are stored in reverse order, we use the
  // opposite endianness to the target.
  void setBeforeBytes(uint64_t Pos, uint8_t Size) {
    assert(Pos >= 8 * minBeforeBytes());
    if (IsBigEndian)
      TM->Bits->Before.setLE(Pos - 8 * minBeforeBytes(), RetVal, Size);
    else
      TM->Bits->Before.setBE(Pos - 8 * minBeforeBytes(), RetVal, Size);
  }

  // Set the bytes at position Pos after the address point to RetVal.
  void setAfterBytes(uint64_t Pos, uint8_t Size) {
    assert(Pos >= 8 * minAfterBytes());
    if (IsBigEndian)
      TM->Bits->After.setBE(Pos - 8 * minAfterBytes(), RetVal, Size);
    else
      TM->Bits->After.setLE(Pos - 8 * minAfterBytes(), RetVal, Size);
  }
};

namespace {
// A slot in a set of virtual tables. The TypeID identifies the set of virtual
// tables, and the ByteOffset is the offset in bytes from the address point to
// the virtual function pointer.
struct VTableSlot {
  Metadata *TypeID;
  uint64_t ByteOffset;
};
} // end anonymous namespace

namespace llvm {

template <> struct DenseMapInfo<VTableSlot> {
  static VTableSlot getEmptyKey() {
    return {DenseMapInfo<Metadata *>::getEmptyKey(),
            DenseMapInfo<uint64_t>::getEmptyKey()};
  }
  static VTableSlot getTombstoneKey() {
    return {DenseMapInfo<Metadata *>::getTombstoneKey(),
            DenseMapInfo<uint64_t>::getTombstoneKey()};
  }
  static unsigned getHashValue(const VTableSlot &I) {
    return DenseMapInfo<Metadata *>::getHashValue(I.TypeID) ^
           DenseMapInfo<uint64_t>::getHashValue(I.ByteOffset);
  }
  static bool isEqual(const VTableSlot &LHS, const VTableSlot &RHS) {
    return LHS.TypeID == RHS.TypeID && LHS.ByteOffset == RHS.ByteOffset;
  }
};
} // namespace llvm

struct VirtualCallSite {
  Value *VTable = nullptr;
  CallBase &CB;

  // If non-null, this field points to the associated unsafe use count stored in
  // the DevirtModule::NumUnsafeUsesForTypeTest map below. See the description
  // of that field for details.
  unsigned *NumUnsafeUses = nullptr;
};

// Call site information collected for a specific VTableSlot and possibly a list
// of constant integer arguments. The grouping by arguments is handled by the
// VTableSlotInfo class.
struct CallSiteInfo {
  /// The set of call sites for this slot. Used during regular LTO and the
  /// import phase of ThinLTO (as well as the export phase of ThinLTO for any
  /// call sites that appear in the merged module itself); in each of these
  /// cases we are directly operating on the call sites at the IR level.
  std::vector<VirtualCallSite> CallSites;

  /// Whether all call sites represented by this CallSiteInfo, including those
  /// in summaries, have been devirtualized. This starts off as true because a
  /// default constructed CallSiteInfo represents no call sites.
  bool AllCallSitesDevirted = true;

  // These fields are used during the export phase of ThinLTO and reflect
  // information collected from function summaries.

  /// Whether any function summary contains an llvm.assume(llvm.type.test) for
  /// this slot.
  bool SummaryHasTypeTestAssumeUsers = false;

  /// CFI-specific: a vector containing the list of function summaries that use
  /// the llvm.type.checked.load intrinsic and therefore will require
  /// resolutions for llvm.type.test in order to implement CFI checks if
  /// devirtualization was unsuccessful. If devirtualization was successful, the
  /// pass will clear this vector by calling markDevirt(). If at the end of the
  /// pass the vector is non-empty, we will need to add a use of llvm.type.test
  /// to each of the function summaries in the vector.
  std::vector<FunctionSummary *> SummaryTypeCheckedLoadUsers;
  std::vector<FunctionSummary *> SummaryTypeTestAssumeUsers;

  bool isExported() const {
    return SummaryHasTypeTestAssumeUsers ||
           !SummaryTypeCheckedLoadUsers.empty();
  }

  void addSummaryTypeCheckedLoadUser(FunctionSummary *FS) {
    SummaryTypeCheckedLoadUsers.push_back(FS);
    AllCallSitesDevirted = false;
  }

  void addSummaryTypeTestAssumeUser(FunctionSummary *FS) {
    SummaryTypeTestAssumeUsers.push_back(FS);
    SummaryHasTypeTestAssumeUsers = true;
    AllCallSitesDevirted = false;
  }

  void markDevirt() {
    AllCallSitesDevirted = true;

    // As explained in the comment for SummaryTypeCheckedLoadUsers.
    SummaryTypeCheckedLoadUsers.clear();
  }
};

// Call site information collected for a specific VTableSlot.
struct VTableSlotInfo {
  // The set of call sites which do not have all constant integer arguments
  // (excluding "this").
  CallSiteInfo CSInfo;

  // The set of call sites with all constant integer arguments (excluding
  // "this"), grouped by argument list.
  std::map<std::vector<uint64_t>, CallSiteInfo> ConstCSInfo;

  void addCallSite(Value *VTable, CallBase &CB, unsigned *NumUnsafeUses);

private:
  CallSiteInfo &findCallSiteInfo(CallBase &CB);
};

struct DevirtModule {
  Module &M;
  // function_ref<AAResults &(Function &)> AARGetter;
  // function_ref<DominatorTree &(Function &)> LookupDomTree;

  ModuleSummaryIndex *ExportSummary;
  // const ModuleSummaryIndex *ImportSummary;

  IntegerType *Int8Ty;
  PointerType *Int8PtrTy;
  IntegerType *Int32Ty;
  IntegerType *Int64Ty;
  IntegerType *IntPtrTy;
  /// Sizeless array type, used for imported vtables. This provides a signal
  /// to analyzers that these imports may alias, as they do for example
  /// when multiple unique return values occur in the same vtable.
  ArrayType *Int8Arr0Ty;

  bool RemarksEnabled = false;

  MapVector<VTableSlot, VTableSlotInfo> CallSlots;

  // Calls that have already been optimized. We may add a call to multiple
  // VTableSlotInfos if vtable loads are coalesced and need to make sure not to
  // optimize a call more than once.
  SmallPtrSet<CallBase *, 8> OptimizedCalls;

  // This map keeps track of the number of "unsafe" uses of a loaded function
  // pointer. The key is the associated llvm.type.test intrinsic call generated
  // by this pass. An unsafe use is one that calls the loaded function pointer
  // directly. Every time we eliminate an unsafe use (for example, by
  // devirtualizing it or by applying virtual constant propagation), we
  // decrement the value stored in this map. If a value reaches zero, we can
  // eliminate the type check by RAUWing the associated llvm.type.test call with
  // true.
  std::map<CallInst *, unsigned> NumUnsafeUsesForTypeTest;

  DevirtModule(
      Module &M,
      // function_ref<AAResults &(Function &)> AARGetter,
      //          function_ref<DominatorTree &(Function &)> LookupDomTree,
      ModuleSummaryIndex *ExportSummary
      //         const ModuleSummaryIndex *ImportSummary
      )
      : M(M),
        // AARGetter(AARGetter), LookupDomTree(LookupDomTree),
        ExportSummary(ExportSummary),
        // ImportSummary(ImportSummary),
        Int8Ty(Type::getInt8Ty(M.getContext())),
        Int8PtrTy(Type::getInt8PtrTy(M.getContext())),
        Int32Ty(Type::getInt32Ty(M.getContext())),
        Int64Ty(Type::getInt64Ty(M.getContext())),
        IntPtrTy(M.getDataLayout().getIntPtrType(M.getContext(), 0)),
        Int8Arr0Ty(ArrayType::get(Type::getInt8Ty(M.getContext()), 0)) {
    // assert(!(ExportSummary && ImportSummary));
  }

  void
  scanTypeTestUsers(Function *TypeTestFunc,
                    DenseMap<Metadata *, std::set<TypeMemberInfo>> &TypeIdMap);
  void scanTypeCheckedLoadUsers(Function *TypeCheckedLoadFunc);

  void buildTypeIdentifierMap(
      std::vector<VTableBits> &Bits,
      DenseMap<Metadata *, std::set<TypeMemberInfo>> &TypeIdMap);

  bool
  tryFindVirtualCallTargets(std::vector<VirtualCallTarget> &TargetsForSlot,
                            const std::set<TypeMemberInfo> &TypeMemberInfos,
                            uint64_t ByteOffset,
                            ModuleSummaryIndex *ExportSummary);

  // Apply the summary resolution for Slot to all virtual calls in SlotInfo.
  void importResolution(VTableSlot Slot, VTableSlotInfo &SlotInfo);

  std::map<llvm::CallBase *, std::vector<llvm::Function *>> run();

  // Look up the corresponding ValueInfo entry of `TheFn` in `ExportSummary`.
  //
  // Caller guarantees that `ExportSummary` is not nullptr.
  static ValueInfo lookUpFunctionValueInfo(Function *TheFn,
                                           ModuleSummaryIndex *ExportSummary);

  // Returns true if the function definition must be unreachable.
  //
  // Note if this helper function returns true, `F` is guaranteed
  // to be unreachable; if it returns false, `F` might still
  // be unreachable but not covered by this helper function.
  //
  // Implementation-wise, if function definition is present, IR is analyzed; if
  // not, look up function flags from ExportSummary as a fallback.
  static bool mustBeUnreachableFunction(Function *const F,
                                        ModuleSummaryIndex *ExportSummary);
};

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
    if (!TM.Bits->GV->isConstant()) {
      return false;
    }
    // TODO is this safe?
    // We cannot perform whole program devirtualization analysis on a vtable
    // with public LTO visibility.

    /*if (TM.Bits->GV->getVCallVisibility() ==
        GlobalObject::VCallVisibilityPublic)
    {
return false;
    }*/
    // we dont perform devirtualization we only need the analysis result

    Constant *Ptr = getPointerAtOffset(TM.Bits->GV->getInitializer(),
                                       TM.Offset + ByteOffset, M);
    if (!Ptr) {
      return false;
    }

    auto Fn = dyn_cast<Function>(Ptr->stripPointerCasts());
    if (!Fn) {
      return false;
    }

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

std::map<llvm::CallBase *, std::vector<llvm::Function *>> DevirtModule::run() {

  std::map<llvm::CallBase *, std::vector<llvm::Function *>> result;

  Function *TypeTestFunc =
      M.getFunction(Intrinsic::getName(Intrinsic::type_test));
  Function *TypeCheckedLoadFunc =
      M.getFunction(Intrinsic::getName(Intrinsic::type_checked_load));
  Function *AssumeFunc = M.getFunction(Intrinsic::getName(Intrinsic::assume));

  // TODO what is actually the difference between those??
  if (not TypeTestFunc)
    TypeTestFunc =
        M.getFunction(Intrinsic::getName(Intrinsic::public_type_test));

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
      // populate the result
      errs() << "Populate Result\n";
      std::vector<Function *> possible_targets;
      for (auto slot : TargetsForSlot) {
        possible_targets.push_back(slot.Fn);
      }
      VTableSlotInfo &CSInfo = S.second;
      for (auto &cinfo : CSInfo.CSInfo.CallSites) {
        // for calls
        auto *call = &cinfo.CB;
        if (result.find(call) == result.end()) {
          result[call] = possible_targets;
        } else {
          assert(false && "Multiple Analysis results for the same callsite??");
        }
      }
    }
  }

  return result;
}

void DevirtModule::scanTypeTestUsers(
    Function *TypeTestFunc,
    DenseMap<Metadata *, std::set<TypeMemberInfo>> &TypeIdMap) {

  // Find all virtual calls via a virtual table pointer %p under an assumption
  // of the form llvm.assume(llvm.type.test(%p, %md)). This indicates that %p
  // points to a member of the type identifier %md. Group calls by (type ID,
  // offset) pair (effectively the identity of the virtual function) and store
  // to CallSlots.
  for (Use &U : llvm::make_early_inc_range(TypeTestFunc->uses())) {
    auto *CI = dyn_cast<CallInst>(U.getUser());
    if (!CI)
      continue;

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

VirtualCallTarget::VirtualCallTarget(Function *Fn, const TypeMemberInfo *TM)
    : Fn(Fn), TM(TM),
      IsBigEndian(Fn->getParent()->getDataLayout().isBigEndian()),
      WasDevirt(false) {}

DevirtAnalysis::DevirtAnalysis(llvm::Module &M) {
  DevirtModule dvm =
      DevirtModule(M, analysis_results->get_module_summary_index());

  result_map = dvm.run();
}