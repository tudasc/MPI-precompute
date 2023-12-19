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

#include "devirt_analysis.h"
#include "ptr_info.h"
#include "taintedValue.h"
#include <numeric>
#include <regex>
#include <utility>

#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "analysis_results.h"
#include "llvm/Demangle/Demangle.h"

#include "VtableManager.h"

class PrecalculationFunctionAnalysis {
public:
  // analysis Part
  explicit PrecalculationFunctionAnalysis(llvm::Function *F) : func(F) {
    assert(not F->isDeclaration() && "Cannot analyze external function");
  };
  void add_relevant_args(const std::set<unsigned int> &new_args_to_use) {
    std::copy(new_args_to_use.begin(), new_args_to_use.end(),
              std::inserter(args_to_use, args_to_use.begin()));
  }
  std::set<unsigned int> args_to_use = {};
  llvm::Function *func;
};

class PrecalculationAnalysis {
public:
  PrecalculationAnalysis(llvm::Module &M, llvm::Function *entry_point)
      : M(M), entry_point(entry_point), virtual_call_sites(DevirtAnalysis(M)) {
    find_functions_called_indirect();
  };

  void add_precalculations(const std::vector<llvm::CallBase *> &to_precompute);

private:
  std::set<std::shared_ptr<PrecalculationFunctionAnalysis>>
      functions_to_include;

public:
  const std::set<std::shared_ptr<PrecalculationFunctionAnalysis>> &
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

  std::shared_ptr<TaintedValue>
  insert_tainted_value(llvm::Value *v,
                       const std::shared_ptr<TaintedValue> &from = nullptr);

  std::shared_ptr<TaintedValue> insert_tainted_value(llvm::Value *v,
                                                     TaintReason reason);

public:
  std::shared_ptr<TaintedValue> get_taint_info(llvm::Value *v) const {
    assert(is_tainted(v));
    return *std::find_if(tainted_values.begin(), tainted_values.end(),
                         [&v](const auto &vv) { return vv->v == v; });
  }

  bool is_func_included_in_precompute(llvm::Function *F) const {
    return std::find_if(functions_to_include.begin(),
                        functions_to_include.end(), [F](const auto p) {
                          return F == p->func;
                        }) != functions_to_include.end();
  }

  // collect all call sites that can call F respecting indirect calls
  std::vector<llvm::CallBase *> get_callsites(llvm::Function *F) {
    // TODO IMPLEMENT
    assert(false);
    return {};
  }

private:
  std::shared_ptr<PrecalculationFunctionAnalysis>
  insert_functions_to_include(llvm::Function *func);

  // TODO we need some kind of heuristic to check if precalculation of all msg
  // tags seems to be worth it
  //  or if e.g. for some reason a compute heavy loop was included as well

  std::set<llvm::Function *> functions_that_may_be_called_indirect;

  void find_all_tainted_vals();
  void find_functions_called_indirect();
  // we need a function to re-initialize all globals that may be overwritten

  void print_analysis_result_remarks();
  void debug_printings();

  void visit_val(std::shared_ptr<TaintedValue> v);
  void visit_arg(std::shared_ptr<TaintedValue> arg_info);

  void visit_load(const std::shared_ptr<TaintedValue> &load_info);
  void visit_store(const std::shared_ptr<TaintedValue> &store_info);
  void visit_store_from_value(const std::shared_ptr<TaintedValue> &store_info);
  void visit_store_from_ptr(const std::shared_ptr<TaintedValue> &store_info);
  void visit_gep(const std::shared_ptr<TaintedValue> &gep_info);
  void visit_phi(const std::shared_ptr<TaintedValue> &phi_info);

  void visit_call(std::shared_ptr<TaintedValue> call_info);
  void visit_call_from_ptr(llvm::CallBase *call,
                           const std::shared_ptr<TaintedValue> &ptr);
  void visit_ptr_usages(std::shared_ptr<TaintedValue> ptr);
  void visit_ptr_ret(const std::shared_ptr<TaintedValue> &ptr,
                     llvm::ReturnInst *ret);

public:
  bool is_tainted(llvm::Value *v) const {
    return std::find_if(tainted_values.begin(), tainted_values.end(),
                        [&v](const auto &vv) { return vv->v == v; }) !=
           tainted_values.end();
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

  void taint_all_indirect_call_args(llvm::Function *func, unsigned int argNo,
                                    std::shared_ptr<TaintedValue> arg_info);
  void taint_all_indirect_calls(llvm::Function *func);

  bool is_invoke_necessary_for_control_flow(llvm::InvokeInst *invoke) const;
  bool is_invoke_exception_case_needed(llvm::InvokeInst *invoke) const;

private:
  void
  analyze_ptr_usage_in_std(llvm::CallBase *call,
                           const std::shared_ptr<TaintedValue> &ptr_arg_info);

  std::vector<llvm::Function *> get_possible_call_targets(llvm::CallBase *call);
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

inline bool is_func_from_std(llvm::Function *func) {

  auto demangled = llvm::demangle(func->getName().str());

  // errs() << "Test if in std:\n" <<demangled << "\n";

  // startswith std::
  if (demangled.rfind("std::", 0) == 0) {
    return true;
  }
  // for some templates e.g.  unsigned long const& std::min<unsigned
  // long>(unsigned long const&, unsigned long const&) the return type is part
  // of the mangled name
  std::regex regex_pattern_std(
      "^((unsigned )?(((int)|(long)|(float)|(double)))?( const)?(&)? )?"
      "std::(.+)");
  if (std::regex_match(demangled, regex_pattern_std)) {
    return true;
  }

  // internals of gnu implementation
  if (demangled.rfind("__gnu_cxx::", 0) == 0) {
    return true;
  }
  std::regex regex_pattern_gnu(
      "^((unsigned )?(((int)|(long)|(float)|(double)))?(const)?(&)? )?"
      "__gnu_cxx::(.+)");
  if (std::regex_match(demangled, regex_pattern_gnu)) {
    return true;
  }

  // C API
  llvm::LibFunc lib_func;
  bool in_lib = analysis_results->getTLI()->getLibFunc(*func, lib_func);
  if (in_lib) {
    return true;
  }

  // more like a stack ptr than a function call
  if (func->getName() == "__errno_location") {
    return true;
  }

  return false;
}

inline bool is_call_to_std(llvm::CallBase *call) {
  if (call->isIndirectCall()) {
    return false;
  }

  return is_func_from_std(call->getCalledFunction());
}

#endif // MACH_PRECALCULATIONS_H_