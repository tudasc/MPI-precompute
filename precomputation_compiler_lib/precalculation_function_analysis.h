
#ifndef PRECALCULATION_FUNCTION_ANALYSIS_H
#define PRECALCULATION_FUNCTION_ANALYSIS_H

#include "Openmp_region.h"
#include "mpi_functions.h"
#include "ptr_info.h"
#include <llvm/IR/Intrinsics.h>

#include <memory>

class PrecalculationFunctionCopy;
class PrecalculationAnalysis;

class PrecalculationFunctionAnalysis
    : public std::enable_shared_from_this<PrecalculationFunctionAnalysis> {
public:
  // analysis Part
  explicit PrecalculationFunctionAnalysis(llvm::Function *F,
                                          PrecalculationAnalysis *precalc);

  void add_relevant_args(const std::set<unsigned int> &new_args_to_use) {
    std::copy(new_args_to_use.begin(), new_args_to_use.end(),
              std::inserter(args_to_use, args_to_use.begin()));
  }

  std::set<unsigned int> args_to_use = {};
  llvm::Function *func;
  PrecalculationAnalysis *precalculatioanalysis;
  std::set<llvm::GlobalValue *> aliases;
  const std::set<llvm::GlobalValue *> &getAliases() const { return aliases; }

  bool include_in_precompute = false;

  // can this function throw an exception where the except case needs to be
  // handled in precompute? some funcs like malloc or writing to stdout can
  // except causing the control flow to divert from the precomputation but these
  // exceptions are so harmful that precompute need to abort anyway, so we don't
  // actually need to handle it during precompute (and check if the precompute
  // handle it)
  bool can_except_in_precompute = true;
  bool analysis_except_in_precompute =
      false; // used avoid endless recursion on recursive call chains

  // always include all calls to this func (it must always execute aka contains
  // the register precompute cals) for other functions, if the writes done by
  // the func are not important anymore, we can skip calling them
  bool include_all_callsites = false;

  bool is_openmp_parallel = false;
  std::shared_ptr<ParallelRegion> parallel_region = nullptr;
  bool is_openmp_task = false;
  std::vector<llvm::CallBase *> task_alloc_calls = {};

  // used outside of call instructions
  bool is_func_ptr_captured;

  // all call sites that can call F respecting indirect calls
  std::set<llvm::CallBase *> callsites;
  // all possible callees called by F respecting indirect calls
  std::set<std::weak_ptr<PrecalculationFunctionAnalysis>, std::owner_less<>>
      callees;

  // set of ptrs possibly written or Read when calling this func
  // contains direct read and writes
  std::set<std::shared_ptr<PtrUsageInfo>> ptr_read;
  std::set<std::shared_ptr<PtrUsageInfo>> ptr_written;

  // includes al callees
  std::set<std::shared_ptr<PtrUsageInfo>> getPtrRead_recursive() const;

  std::set<std::shared_ptr<PtrUsageInfo>> getPtrWritten_recursive() const;

private:
  void getPtrRead_recursive_impl(
      std::set<std::shared_ptr<PtrUsageInfo>> &result,
      std::set<std::shared_ptr<const PrecalculationFunctionAnalysis>> &visited)
      const;

  void getPtrWritten_recursive_impl(
      std::set<std::shared_ptr<PtrUsageInfo>> &result,
      std::set<std::shared_ptr<const PrecalculationFunctionAnalysis>> &visited)
      const;

public:
  // invalidate the analysis of call sites of this function
  void re_visit_callsites();

  void add_ptr_read(const std::shared_ptr<PtrUsageInfo> &read) {
    // assert(read->isReadFrom());
    // one may insert them before the read/write is analyzed
    auto pair = ptr_read.insert(read);
    if (pair.second) {
      // was inserted
      re_visit_callsites();
    }
  }

  void add_ptr_write(const std::shared_ptr<PtrUsageInfo> &write) {
    // assert(write->isWrittenTo());
    // one may insert them before the read/write is analyzed
    auto pair = ptr_written.insert(write);
    if (pair.second) {
      // was inserted
      re_visit_callsites();
    }
  }

  void analyze_can_except_in_precompute(
      const PrecalculationAnalysis *precompute_analysis);
};

inline bool should_exclude_function_for_debugging(llvm::Function *func) {
  if (is_mpi_function(func)) {
    return true;
  }
  return false;
}

// only gets the name of a function if a demangled name contains a return
// param or template args
std::string get_function_name(const std::string &demangled_name);

// we ignore those intrinsics for precompute we dont need to call them
inline bool should_ignore_intrinsic(llvm::Intrinsic::ID id) {
  return
      // intrinsics serving as additional annotations to the IR:
      id == llvm::Intrinsic::lifetime_start ||
      id == llvm::Intrinsic::lifetime_end || id == llvm::Intrinsic::type_test ||
      id == llvm::Intrinsic::public_type_test ||
      id == llvm::Intrinsic::assume ||
      id == llvm::Intrinsic::type_checked_load; // NOLINT
}

// we consider this intrinsics as safe to call during precompute
inline bool should_call_intrinsic(llvm::Intrinsic::ID id) {
  return
      // it is also safe to call ignored intrinsics
      // they only serve as IR annotations anyway
      should_ignore_intrinsic(id) ||
      // std:: functions
      // (https://llvm.org/docs/LangRef.html#standard-c-c-library-intrinsics):
      id == llvm::Intrinsic::abs || id == llvm::Intrinsic::smax ||
      id == llvm::Intrinsic::smin || id == llvm::Intrinsic::umax ||
      id == llvm::Intrinsic::umin || id == llvm::Intrinsic::memcpy ||
      id == llvm::Intrinsic::memcpy_inline || id == llvm::Intrinsic::memmove ||
      id == llvm::Intrinsic::memset || id == llvm::Intrinsic::memset_inline ||
      id == llvm::Intrinsic::sqrt || id == llvm::Intrinsic::powi ||
      id == llvm::Intrinsic::sin || id == llvm::Intrinsic::cos ||
      id == llvm::Intrinsic::pow || id == llvm::Intrinsic::exp ||
      id == llvm::Intrinsic::exp2 || id == llvm::Intrinsic::exp ||
      id == llvm::Intrinsic::log || id == llvm::Intrinsic::log10 ||
      id == llvm::Intrinsic::log2 || id == llvm::Intrinsic::fma ||
      id == llvm::Intrinsic::fabs || id == llvm::Intrinsic::minnum ||
      id == llvm::Intrinsic::maxnum || id == llvm::Intrinsic::minimum ||
      id == llvm::Intrinsic::maximum || id == llvm::Intrinsic::copysign ||
      id == llvm::Intrinsic::floor || id == llvm::Intrinsic::ceil ||
      id == llvm::Intrinsic::trunc || id == llvm::Intrinsic::rint ||
      id == llvm::Intrinsic::nearbyint || id == llvm::Intrinsic::round ||
      id == llvm::Intrinsic::roundeven || id == llvm::Intrinsic::lround ||
      id == llvm::Intrinsic::llround || id == llvm::Intrinsic::lrint ||
      id == llvm::Intrinsic::llrint ||
      // specialized arithmetic
      id == llvm::Intrinsic::fmuladd || id == llvm::Intrinsic::fma ||
      id == llvm::Intrinsic::umul_with_overflow ||
      // exception handling:
      id == llvm::Intrinsic::eh_typeid_for ||
      // stack reading
      id == llvm::Intrinsic::returnaddress ||
      id == llvm::Intrinsic::addressofreturnaddress ||
      id == llvm::Intrinsic::sponentry || id == llvm::Intrinsic::frameaddress ||
      id == llvm::Intrinsic::threadlocal_address ||

      // vector instructions
      llvm::Intrinsic::getName(id).starts_with("llvm.x86.sse"); // NOLINT
}

#endif // PRECALCULATION_FUNCTION_ANALYSIS_H
