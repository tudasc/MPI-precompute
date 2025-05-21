#ifndef STD_FUNCS_H
#define STD_FUNCS_H

#include "devirt_analysis.h"
#include "llvm/IR/Function.h"

#include <llvm/Demangle/Demangle.h>
#include <llvm/IR/InstrTypes.h>
#include <regex>

// call before creating a PrecomputeAnalysis object if this is needed
void allow_function_prefixes_to_be_called_in_precompute(
    const std::vector<std::string> &prefixes_to_allow);

inline bool is_free(const llvm::Function *func) {
  assert(func);
  // operator delete
  if (func->getName() == "_ZdlPv") {
    return true;
  }
  if (func->getName() == "free") {
    return true;
  }
  return false;
}

inline bool is_free(const llvm::CallBase *call) {
  if (call->isIndirectCall()) {
    return false;
  }
  auto *func = call->getCalledFunction();
  if (!call->isIndirectCall() && !func) {
    func = llvm::cast<llvm::Function>(call->getCalledOperand());
  }

  return is_free(func);
}

bool is_interaction_with_cout(llvm::CallBase *call);

inline bool is_allocation(llvm::Function *func) {
  assert(func);
  // operator new
  if (func->getName() == "_Znwm") {
    return true;
  }
  if (func->getName() == "malloc") {
    return true;
  }
  if (func->getName() == "calloc") {
    return true;
  }
  return false;
}

inline bool is_allocation(llvm::CallBase *call) {
  if (call->isIndirectCall()) {
    return false;
  }
  return is_allocation(call->getCalledFunction());
}

bool is_func_from_std(llvm::Function *func);

// this function is used, in order to signal a virtual call to some function of
// std::
llvm::Function *get_std_dummy_func(llvm::Module *M);

inline bool is_name_from_std(const std::string &name) {
  auto demangled = llvm::demangle(name);
  // startswith std::
  if (demangled.rfind("std::", 0) == 0) {
    return true;
  }

  std::regex regex_pattern_std("^(VTT for )?std::(.+)");
  if (std::regex_match(demangled, regex_pattern_std)) {
    return true;
  }

  if (std::regex_match(demangled,
                       std::regex("^(typeinfo( name)? for )?std::(.+)"))) {
    return true;
  }

  if (name == "__dso_handle") {
    return true;
  }

  // these are set if calling fflush on them
  if (name == "stdout") {
    return true;
  }
  if (name == "stderr") {
    return true;
  }

  return false;
}

// we should not mess around with the globals defined by std::
inline bool is_global_from_std(llvm::GlobalValue *global) {
  assert(global);
  if (auto *f = llvm::dyn_cast<llvm::Function>(global)) {
    return is_func_from_std(f);
  }

  return is_name_from_std(global->getName().str());
}

inline bool is_call_to_std(llvm::CallBase *call) {
  if (call->isIndirectCall()) {
    auto tgts = DevirtAnalysis::get_possible_call_targets(call);
    if (tgts.empty()) {
      return false;
    }
    return std::all_of(tgts.begin(), tgts.end(),
                       [](auto t) { return is_func_from_std(t); });
  }
  auto *func = call->getCalledFunction();
  if (!call->isIndirectCall() && !func) {
    // need that for -std=cnu89
    func = llvm::cast<llvm::Function>(call->getCalledOperand());
  }

  return is_func_from_std(func);
}

#endif // STD_FUNCS_H
