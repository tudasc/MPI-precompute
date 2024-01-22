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
#ifndef MACH_PRECALCULATIONS_H_
#define MACH_PRECALCULATIONS_H_

#include <numeric>
#include <regex>
#include <utility>

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "VtableManager.h"
#include "analysis_results.h"
#include "devirt_analysis.h"
#include "ptr_info.h"
#include "taintedValue.h"

class PrecalculationAnalysis;

class PrecalculationFunctionAnalysis
    : public std::enable_shared_from_this<PrecalculationFunctionAnalysis> {
public:
  // analysis Part
  explicit PrecalculationFunctionAnalysis(
      llvm::Function *F, std::weak_ptr<PrecalculationAnalysis> precalc)
      : func(F), precalculatioanalysis(std::move(precalc)) {
    // assert(not F->isDeclaration() && "Cannot analyze external function");

    is_func_ptr_captured = false;
    aliases.insert(func);

    for (auto *u : func->users()) {
      if (auto *alias = llvm::dyn_cast<llvm::GlobalAlias>(u)) {
        assert(alias->getAliasee() == func);
        aliases.insert(alias);

        if (not alias->user_empty()) {
          llvm::errs() << "Alias is used\n";
          for (auto *auu : alias->users()) {
            auu->dump();
          }
          llvm::errs() << "currently not supported\n";
          assert(false);
        }

        continue;
      }
      if (not llvm::isa<llvm::CallBase>(u)) {
        is_func_ptr_captured = true;
      }
    }
  };

  void add_relevant_args(const std::set<unsigned int> &new_args_to_use) {
    std::copy(new_args_to_use.begin(), new_args_to_use.end(),
              std::inserter(args_to_use, args_to_use.begin()));
  }
  std::set<unsigned int> args_to_use = {};
  llvm::Function *func;
  std::weak_ptr<PrecalculationAnalysis> precalculatioanalysis;
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

  // used outside of call instructions
  bool is_func_ptr_captured;

  // all call sites that can call F respecting indirect calls
  std::set<llvm::CallBase *> callsites;
  // all possible callees called by F respecting indirect calls
  std::set<std::weak_ptr<PrecalculationFunctionAnalysis>, std::owner_less<>>
      callees;

  // TODO do we actually need this?
  // set of ptrs possibly written or Read when calling this func
  // contains direct read and writes
  // TODO Getter to receive the recursive set including all callees
  std::set<std::shared_ptr<PtrUsageInfo>> ptr_read;
  std::set<std::shared_ptr<PtrUsageInfo>> ptr_written;

  // invalidate the analysis of call sites of this function
  void re_visit_callsites();

  void add_ptr_read(const std::shared_ptr<PtrUsageInfo> &read) {
    assert(read->isReadFrom());
    auto pair = ptr_read.insert(read);
    if (pair.second) { // was inserted
      re_visit_callsites();
    }
  }

  void add_ptr_write(const std::shared_ptr<PtrUsageInfo> &write) {
    assert(write->isWrittenTo());
    auto pair = ptr_written.insert(write);
    if (pair.second) { // was inserted
      re_visit_callsites();
    }
  }

  void analyze_can_except_in_precompute(
      const PrecalculationAnalysis *precompute_analysis);
};

class PrecalculationAnalysis
    : std::enable_shared_from_this<PrecalculationAnalysis> {
public:
  PrecalculationAnalysis(llvm::Module &M, llvm::Function *entry_point)
      : M(M), entry_point(entry_point), virtual_call_sites(DevirtAnalysis(M)) {
    analyze_functions();
  };

  void add_precalculations(const std::vector<llvm::CallBase *> &to_precompute);

private:
  std::map<llvm::Function *, std::shared_ptr<PrecalculationFunctionAnalysis>>
      function_analysis;
  void analyze_functions();

public:
  std::set<std::shared_ptr<PrecalculationFunctionAnalysis>>
  getFunctionsToInclude() const;
  llvm::Function *getEntryPoint() const;
  const std::vector<llvm::CallBase *> &getToReplaceWithEnvelopeRegister() const;

public:
private:
  llvm::Module &M;
  llvm::Function *entry_point;

  DevirtAnalysis virtual_call_sites;

  std::vector<llvm::CallBase *> to_replace_with_envelope_register;
  std::set<std::shared_ptr<TaintedValue>> tainted_values;

  void include_value_in_precompute(const std::shared_ptr<TaintedValue> &);
  std::shared_ptr<TaintedValue>
  insert_tainted_value(llvm::Value *v,
                       const std::shared_ptr<TaintedValue> &from = nullptr,
                       bool needed_from = true);

  std::shared_ptr<TaintedValue> insert_tainted_value(llvm::Value *v,
                                                     TaintReason reason);

public:
  std::shared_ptr<TaintedValue> get_taint_info(llvm::Value *v) const {
    assert(is_tainted(v));
    return *std::find_if(tainted_values.begin(), tainted_values.end(),
                         [&v](const auto &vv) { return vv->v == v; });
  }

  bool is_func_included_in_precompute(llvm::Function *F) const {
    return function_analysis.at(F)->include_in_precompute;
  }
  std::shared_ptr<PrecalculationFunctionAnalysis>
  get_function_analysis(llvm::Function *F) const {
    return function_analysis.at(F);
  }

private:
  void insert_functions_to_include(llvm::Function *func);

  // TODO we need some kind of heuristic to check if precalculation of all msg
  // tags seems to be worth it
  //  or if e.g. for some reason a compute heavy loop was included as well

  // std::set<llvm::Function *> functions_that_may_be_called_indirect;

  void find_all_tainted_vals();
  // we need a function to re-initialize all globals that may be overwritten

  void print_analysis_result_remarks();
  void debug_printings();

  void visit_val(const std::shared_ptr<TaintedValue> &v);
  void visit_arg(const std::shared_ptr<TaintedValue> &arg_info);

  void visit_load(const std::shared_ptr<TaintedValue> &load_info);
  void visit_store(const std::shared_ptr<TaintedValue> &store_info);
  void visit_store_from_value(const std::shared_ptr<TaintedValue> &store_info);
  void visit_store_from_ptr(const std::shared_ptr<TaintedValue> &store_info);
  void visit_gep(const std::shared_ptr<TaintedValue> &gep_info);
  void visit_phi(const std::shared_ptr<TaintedValue> &phi_info);

  void visit_call(const std::shared_ptr<TaintedValue> &call_info);
  void visit_call_from_ptr(llvm::CallBase *call,
                           const std::shared_ptr<TaintedValue> &ptr);
  void visit_ptr_usages(const std::shared_ptr<TaintedValue> &ptr);
  void visit_ptr_ret(const std::shared_ptr<TaintedValue> &ptr,
                     llvm::ReturnInst *ret);

  bool is_store_important(llvm::Instruction *inst,
                          const std::shared_ptr<PtrUsageInfo> &ptr_info);
  bool is_store_important(llvm::CallBase *call,
                          const std::shared_ptr<PtrUsageInfo> &ptr_info);
  bool is_store_important(llvm::StoreInst *store,
                          const std::shared_ptr<PtrUsageInfo> &ptr_info);

  // materialize call
  void include_call_to_std(std::shared_ptr<TaintedValue> call_info);

public:
  bool is_tainted(llvm::Value *v) const {
    return std::find_if(tainted_values.begin(), tainted_values.end(),
                        [&v](const auto &vv) { return vv->v == v; }) !=
           tainted_values.end();
  }

public:
  bool is_included_in_precompute(llvm::Value *v) const {
    return std::find_if(tainted_values.begin(), tainted_values.end(),
                        [&v](const auto &vv) {
                          return (vv->v == v && vv->isIncludeInPrecompute());
                        }) != tainted_values.end();
  }

  template <class container>
  unsigned int get_num_tainted(container vals) const {
    return std::accumulate(
        vals.begin(), vals.end(), (unsigned int)0,
        [this](auto accu, auto v) { return accu + is_tainted(v); });
  };

  template <class container> bool are_all_tainted(container vals) const {
    return get_num_tainted(vals) == vals.size();
  }

  template <class container> bool is_none_tainted(container vals) const {
    return !std::accumulate(
        vals.begin(), vals.end(), false,
        [this](auto accu, auto v) { return accu || is_tainted(v); });
  };

  bool is_retval_of_call_needed(llvm::CallBase *call) const;

  bool is_invoke_necessary_for_control_flow(llvm::InvokeInst *invoke) const;
  bool is_invoke_exception_case_needed(llvm::InvokeInst *invoke) const;
  bool can_except_in_precompute(llvm::CallBase *call) const;

private:
  bool
  is_ptr_usage_in_std_read(llvm::CallBase *call,
                           const std::shared_ptr<TaintedValue> &ptr_arg_info);
  bool
  is_ptr_usage_in_std_write(llvm::CallBase *call,
                            const std::shared_ptr<TaintedValue> &ptr_arg_info);

public:
  std::vector<llvm::Function *>
  get_possible_call_targets(llvm::CallBase *call) const;

private:
  void insert_necessary_control_flow(llvm::Value *v);
};

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

inline bool is_call_to_std(llvm::CallBase *call) {
  if (call->isIndirectCall()) {
    return false;
  }

  return is_func_from_std(call->getCalledFunction());
}

// we should not mess around with the globals defined by std::
inline bool is_global_from_std(llvm::GlobalValue *global) {
  assert(global);
  if (auto *f = llvm::dyn_cast<llvm::Function>(global)) {
    return is_func_from_std(f);
  }

  auto demangled = llvm::demangle(global->getName().str());
  // startswith std::
  if (demangled.rfind("std::", 0) == 0) {
    return true;
  }

  std::regex regex_pattern_std("^(VTT for )?std::(.+)");
  if (std::regex_match(demangled, regex_pattern_std)) {
    return true;
  }

  if (std::regex_match(demangled, std::regex("^(typeinfo for )?std::(.+)"))) {
    return true;
  }

  if (global->getName() == "__dso_handle") {
    return true;
  }
  return false;
}

#endif // MACH_PRECALCULATIONS_H_