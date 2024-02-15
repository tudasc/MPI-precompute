/*
 Copyright 2022 Tim Jammer

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

#include <boost/stacktrace.hpp>

#include <llvm/IR/Verifier.h>
#include <random>
#include <regex>

#include "conflict_detection.h"
#include "devirt_analysis.h"
#include "implementation_specific.h"
#include "mpi_functions.h"
#include "precalculation.h"

#include "llvm/Analysis/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/Casting.h"

#include "llvm/Demangle/Demangle.h"

#include "Precompute_insertion.h"

#include "debug.h"
using namespace llvm;

// for more scrutiny under testing:
// the order of visiting the values should make no difference
// #define SHUFFLE_VALUES_FOR_TESTING

// only gets the name of a function if a demangled name contains a return
// param or template args
std::string get_function_name(const std::string &demangled_name);

// an interaction with std::cout can not except during precompute (any exception
// would be considered fatal anyway) therefore we do not need to analyze the
// interactions with std::cout
bool is_interaction_with_cout(llvm::CallBase *call) {

  if (call->isIndirectCall()) {
    return false; // may except
  }

  if (call->getCalledFunction()->getName() == "printf") {
    return true;
  }

  auto *cout = call->getModule()->getGlobalVariable("_ZSt4cout");
  if (cout) {
    // errs() << "check if interaction with cout:\n";
    // call->dump();

    if (call->arg_size() >= 2 && call->getArgOperand(0) == cout) {
      // assert(is_func_from_std(call->getCalledFunction()));
      // errs() << "TRUE: interaction with cout:\n";
      return true;
    }
    auto name = get_function_name(
        llvm::demangle(call->getCalledFunction()->getName().str()));
    if (name.find("operator<<") != std::string::npos) {
      // if multiple chained usages of operator << operator << will be used on
      // the result of first application of operator<<
      assert(call->arg_size() >= 2);
      if (auto *cc = dyn_cast<CallBase>(call->getArgOperand(0))) {
        // errs() << "Defer to other call: interaction with cout:\n";
        return is_interaction_with_cout(cc);
      }
      if (auto *phi = dyn_cast<PHINode>(call->getArgOperand(0))) {
        // std::all_of
        for (auto &incoming : phi->incoming_values()) {
          if (auto *cc = dyn_cast<CallBase>(&incoming)) {
            // errs() << "Defer to other call: interaction with cout:\n";
            if (not is_interaction_with_cout(cc)) {
              return false;
            }
          } else {
            return false;
          }
        }
        return true;
      }
    }
  }
  // errs() << "FALSE: no interaction with cout:\n";
  return false;
}

void print_needs(const std::shared_ptr<TaintedValue> &parent,
                 unsigned int indent = 0) {
  if (indent > 10 || parent == nullptr || parent->v == nullptr)
    return;

  for (unsigned int i = 0; i < indent; ++i) {
    errs() << "\t";
  }
  parent->v->dump();
  for (const auto &c : parent->needs) {
    print_needs(c, indent + 1);
  }
}

void print_needed_for(const std::shared_ptr<TaintedValue> &child,
                      unsigned int indent = 0) {
  if (indent > 4 || child == nullptr || child->v == nullptr)
    return;

  for (unsigned int i = 0; i < indent; ++i) {
    errs() << "\t";
  }
  child->v->dump();
  for (const auto &p : child->needed_for) {
    print_needed_for(p, indent + 1);
  }
}

void PrecalculationAnalysis::analyze_functions() {
  // create
  for (auto &f : M) {
    function_analysis[&f] =
        std::make_shared<PrecalculationFunctionAnalysis>(&f, this);
  }

  // populate callees and callsites
  for (auto &f : M.functions()) {
    if (not is_func_from_std(&f)) { // dont analyze std's internals
      for (auto I = inst_begin(f), E = inst_end(f); I != E; ++I) {
        if (auto *call = dyn_cast<CallBase>(&*I)) {
          auto targets = get_possible_call_targets(call);
          for (auto *target : targets) {
            function_analysis[target]->callsites.insert(call);
            function_analysis[call->getFunction()]->callees.insert(
                function_analysis[target]);
          }
        }
      }
    }
  }

  for (const auto &pair : function_analysis) {
    pair.second->analyze_can_except_in_precompute(this);
  }
}

bool PrecalculationAnalysis::is_invoke_exception_case_needed(
    llvm::InvokeInst *invoke) const {
  assert(invoke);

  auto *unwindBB = invoke->getUnwindDest();
  std::set<Instruction *> in_unwind;
  for (auto &i : *unwindBB) {
    in_unwind.insert(&i);
    // need to put it into set for sorting
  }
  if (is_none_tainted(in_unwind)) {
    return false;
    // exception handling is not needed;
  }

  bool all_exception_free = true;
  for (auto *tgt : get_possible_call_targets(invoke)) {
    if (get_function_analysis(tgt)->can_except_in_precompute) {
      all_exception_free = false;
      break;
    }
  }
  if (all_exception_free) {
    // cannot yield exception
    return false;
  }
  // may yield an exception
  return true;
}

// True if callee needs to be called as it contains tainted instructions
// or True if callee can raise an exception and the exception handling code is
// actually tainted if exception handling is not tainted, we don't need to
// handle the exception anyway and abortion is fine in this case
bool PrecalculationAnalysis::is_invoke_necessary_for_control_flow(
    llvm::InvokeInst *invoke) const {

  assert(invoke);

  // calling into something we need
  for (auto *tgt : get_possible_call_targets(invoke)) {
    if (function_analysis.at(tgt)->include_in_precompute) {
      return true;
    }
  }

  // check if the exception part is needed
  return is_invoke_exception_case_needed(invoke);
}

bool should_exclude_function_for_debugging(llvm::Function *func) {
  if (is_mpi_function(func)) {
    return true;
  }
  return false;
}

bool is_free(Function *func) {
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

bool is_free(llvm::CallBase *call) {
  if (call->isIndirectCall()) {
    return false;
  }

  return is_free(call->getCalledFunction());
}

void PrecalculationFunctionAnalysis::analyze_can_except_in_precompute(
    const PrecalculationAnalysis *precompute_analysis) {

  // the precompute_analysis object is not fully initialized yet, as we are
  // currently analyzing the functions
  assert(not analysis_except_in_precompute);

  if (func->hasFnAttribute(llvm::Attribute::NoUnwind)) {
    // no exception possible
    can_except_in_precompute = false;
    return;
  }

  if (precompute_analysis->is_allocation(func) // out of mem is fatal
      || is_mpi_function(func) // mpi cannot throw recoverable exceptions
  ) {
    can_except_in_precompute = false;
    return;
  }

  if (func->isIntrinsic()) {
    // intrinsics cannot fail
    can_except_in_precompute = false;
    return;
  }

  if (func->isDeclaration()) {
    // don't know: need to assume it can throw
    // func->dump();
    assert(can_except_in_precompute);
    return;
  }

  if (precompute_analysis->is_func_from_std(func)) {
    // we don't analyze std's internals, assume it can throw
    assert(can_except_in_precompute);
    return;
  }

  analysis_except_in_precompute = true; // this one is currently analyzed
  for (auto &BB : *func) {
    for (auto &inst : BB) {
      if (auto *call = dyn_cast<CallBase>(&inst)) {
        if (not is_interaction_with_cout(call)) {
          for (auto *cc :
               precompute_analysis->get_possible_call_targets(call)) {
            auto callee = precompute_analysis->get_function_analysis(cc);
            assert(callees.find(callee) != callees.end());
            // callee may not be properly initialized yet
            if (not callee->analysis_except_in_precompute) {
              callee->analyze_can_except_in_precompute(precompute_analysis);
            }
            if (callee->can_except_in_precompute &&
                (not(callee == shared_from_this()))) {
              analysis_except_in_precompute = false;
              assert(can_except_in_precompute);
              // TODO better analysis needed??
              //  if they invoke the function that may except they need to
              //  resume exception handling aka if they catch and deal with
              //  exception: they dont throw
              return;
            }
          }
        }
      }
    }
  }

  // no callee can except (this implies that no call to trow was detected)
  analysis_except_in_precompute = false;
  can_except_in_precompute = false;
}
void PrecalculationFunctionAnalysis::re_visit_callsites() {

  // TODO recursively re visit calls that may lead to this callsite??

  assert(precalculatioanalysis);
  for (auto *c : callsites) {

    if (precalculatioanalysis->is_tainted(c)) {
      auto ti = precalculatioanalysis->get_taint_info(c);
      ti->visited = false;
    }
  }
}

void PrecalculationFunctionAnalysis::getPtrRead_recursive_impl(
    std::set<std::shared_ptr<PtrUsageInfo>> &result,
    std::set<std::shared_ptr<const PrecalculationFunctionAnalysis>> &visited)
    const {

  auto pair = visited.insert(shared_from_this());
  if (pair.second) { // was inserted
    for (const auto &ptr : ptr_read) {
      result.insert(ptr);
    }
    for (const auto &c : callees) {
      if (auto callee = c.lock()) {
        callee->getPtrRead_recursive_impl(result, visited);
      }
    }
  }
}

std::set<std::shared_ptr<PtrUsageInfo>>
PrecalculationFunctionAnalysis::getPtrRead_recursive() const {

  std::set<std::shared_ptr<PtrUsageInfo>> result;
  std::set<std::shared_ptr<const PrecalculationFunctionAnalysis>> visited;
  getPtrRead_recursive_impl(result, visited);

  return result;
}

void PrecalculationFunctionAnalysis::getPtrWritten_recursive_impl(
    std::set<std::shared_ptr<PtrUsageInfo>> &result,
    std::set<std::shared_ptr<const PrecalculationFunctionAnalysis>> &visited)
    const {

  auto pair = visited.insert(shared_from_this());
  if (pair.second) { // was inserted
    for (const auto &ptr : ptr_written) {
      result.insert(ptr);
    }
    for (const auto &c : callees) {
      if (auto callee = c.lock()) {
        callee->getPtrWritten_recursive_impl(result, visited);
      }
    }
  }
}

std::set<std::shared_ptr<PtrUsageInfo>>
PrecalculationFunctionAnalysis::getPtrWritten_recursive() const {

  std::set<std::shared_ptr<PtrUsageInfo>> result;
  std::set<std::shared_ptr<const PrecalculationFunctionAnalysis>> visited;
  getPtrWritten_recursive_impl(result, visited);

  return result;
}

void PrecalculationAnalysis::add_precalculations(
    const std::vector<llvm::CallBase *> &to_precompute) {
  to_replace_with_envelope_register = to_precompute;

  for (auto *call : to_precompute) {
    bool is_send = call->getCalledFunction() == mpi_func->mpi_send_init;
    auto *tag = get_tag_value(call, is_send);
    auto *src = get_src_value(call, is_send);

    function_analysis.at(call->getFunction())->include_all_callsites = true;

    auto tag_info = insert_tainted_value(tag, TaintReason::COMPUTE_TAG);
    include_value_in_precompute(tag_info);
    auto dest_info = insert_tainted_value(src, TaintReason::COMPUTE_DEST);
    include_value_in_precompute(dest_info);

    auto call_info = insert_tainted_value(call, TaintReason::CONTROL_FLOW);
    include_value_in_precompute(call_info);
    call_info->visited = true; // dont need to visit the call that will be
                               // replaced with register precompute
  }

  find_all_tainted_vals();

#ifndef NDEBUG
  auto has_error = verifyModule(M, &errs(), nullptr);
  assert(!has_error && "Module Error introduced during analysis part???");
#endif

  insert_precomputation(M, *this);
}

void PrecalculationAnalysis::find_all_tainted_vals() {

  std::vector<std::shared_ptr<TaintedValue>> to_visit;

  do {
    to_visit.clear();
    std::copy_if(tainted_values.begin(), tainted_values.end(),
                 std::back_inserter(to_visit),
                 [](const auto &v) { return !v->visited; });

#ifdef SHUFFLE_VALUES_FOR_TESTING
    // for more scrutiny under testing:
    // the order of visiting the values should make no difference
    std::shuffle(to_visit.begin(), to_visit.end(),
                 std::mt19937(std::random_device()()));
#endif

    for (const auto &v : to_visit) {
      visit_val(v);
    }
  } while (!to_visit.empty());
  // done calculating

  print_analysis_result_remarks();

  // if tags or control flow depend on argc or argv MPI_Init will be tainted
  // (as it writes argc and argv)

  auto pos = std::find_if(
      tainted_values.begin(), tainted_values.end(), [](auto v) -> bool {
        if (auto call = dyn_cast<CallBase>(v->v)) {
          if (call->getCalledFunction() &&
              (call->getCalledFunction() == mpi_func->mpi_init ||
               call->getCalledFunction() == mpi_func->mpi_init_thread)) {
            return true;
          }
        }
        return false;
      });

  tainted_values.erase(*pos);
  // done removing of MPI init
}

void PrecalculationAnalysis::visit_load(
    const std::shared_ptr<TaintedValue> &load_info) {
  assert(not load_info->visited);
  load_info->visited = true;
  auto *load = dyn_cast<LoadInst>(load_info->v);
  assert(load);

  auto loaded_from = insert_tainted_value(load->getPointerOperand(), load_info);
  assert(loaded_from->is_pointer());
  assert(loaded_from->ptr_info);
  loaded_from->ptr_info->setIsReadFrom(load, this);

  if (load_info->is_pointer()) {
    loaded_from->ptr_info->setIsUsedDirectly(true, load_info->ptr_info);
  } else {
    loaded_from->ptr_info->setIsUsedDirectly(true);
  }
}

// Generated by AI
std::string
extract_function_without_namespace(const std::string &qualifiedName) {
  std::istringstream iss(qualifiedName);
  std::vector<std::string> tokens;

  // Tokenize the string by '::'
  std::string token;
  while (std::getline(iss, token, ':')) {
    if (!token.empty()) {
      tokens.push_back(token);
    }
  }

  // Get the last token (function name)
  if (!tokens.empty()) {
    return tokens.back();
  } else {
    // Handle the case where the input string is empty or has no tokens
    return "";
  }
}

void PrecalculationAnalysis::visit_store(
    const std::shared_ptr<TaintedValue> &store_info) {
  assert(not store_info->visited);
  store_info->visited = true;
  auto *store = dyn_cast<StoreInst>(store_info->v);
  assert(store);

  auto ptr = insert_tainted_value(store->getPointerOperand(), store_info, true);
  ptr->ptr_info->setIsUsedDirectly(
      true, store_info->ptr_info); // null if stored value is no ptr
  ptr->ptr_info->setIsWrittenTo(store, this);

  auto func = get_function_analysis(store->getFunction());
  func->add_ptr_write(ptr->ptr_info);

  if (is_store_important(store, ptr->ptr_info)) {
    // we need to include all 3: the ptr, the store, and the stored val
    auto val = insert_tainted_value(store->getValueOperand(), store_info, true);
    include_value_in_precompute(val);
    include_value_in_precompute(store_info);
    include_value_in_precompute(ptr);
  }
}

void PrecalculationAnalysis::visit_gep(
    const std::shared_ptr<TaintedValue> &gep_info) {
  assert(not gep_info->visited);
  gep_info->visited = true;
  auto *gep = dyn_cast<GetElementPtrInst>(gep_info->v);
  assert(gep);

  auto gep_ptr_info = insert_tainted_value(gep->getPointerOperand(), gep_info);

  gep_ptr_info->ptr_info->add_important_member(gep, gep_info->ptr_info);

  // taint all values needed for calculating idx
  for (auto &idx : cast<GetElementPtrInst>(gep_info->v)->indices()) {
    auto *v = dyn_cast<Value>(idx);
    insert_tainted_value(v, gep_info);
  }
}

void PrecalculationAnalysis::visit_phi(
    const std::shared_ptr<TaintedValue> &phi_info) {
  auto *phi = dyn_cast<PHINode>(phi_info->v);
  assert(phi);

  if (phi->getType()->isPointerTy()) {
    auto ptr_info = phi_info->ptr_info;
    if (not ptr_info) {
      phi_info->ptr_info = std::make_shared<PtrUsageInfo>(phi_info);
      ptr_info = phi_info->ptr_info;
    }
    for (unsigned int i = 0; i < phi->getNumOperands(); ++i) {
      auto *vv = phi->getIncomingValue(i);
      auto new_val = insert_tainted_value(vv, phi_info);

      if (new_val->ptr_info) {
        ptr_info->merge_with(new_val->ptr_info);
      } else {
        new_val->ptr_info = ptr_info;
        ptr_info->add_ptr_info_user(new_val);
      }
    }
  } else {
    // no ptr
    for (unsigned int i = 0; i < phi->getNumOperands(); ++i) {
      auto *vv = phi->getIncomingValue(i);
      auto new_val = insert_tainted_value(vv, phi_info);
    }
  }

  phi_info->visited = true;
}

void PrecalculationAnalysis::visit_val(const std::shared_ptr<TaintedValue> &v) {
  // errs() << "Visit\n";
  // v->v->dump();

  // TODO clang tidy repeated branch body (the v->visited = true part)

  if (isa<Constant>(v->v)) {
    // nothing to do for constant
    v->visited = true;
  } else if (isa<LoadInst>(v->v)) {
    visit_load(v);

  } else if (isa<AllocaInst>(v->v)) {
    // nothing to do: visit_ptr_usages is called on all ptrs anyway
    v->visited = true;
  } else if (isa<StoreInst>(v->v)) {
    visit_store(v);

  } else if (auto *op = dyn_cast<BinaryOperator>(v->v)) {
    // arithmetic
    // TODO do we need to exclude some opcodes?
    assert(op->getNumOperands() == 2);
    assert(not op->getType()->isPointerTy());
    insert_tainted_value(op->getOperand(0), v);
    insert_tainted_value(op->getOperand(1), v);
    v->visited = true;
  } else if (auto *uop = dyn_cast<UnaryOperator>(v->v)) {
    // arithmetic
    // TODO do we need to exclude some opcodes?
    assert(uop->getNumOperands() == 1);
    assert(not uop->getType()->isPointerTy());
    insert_tainted_value(uop->getOperand(0), v);
    v->visited = true;
  } else if (auto *cmp = dyn_cast<CmpInst>(v->v)) {
    // cmp
    assert(cmp->getNumOperands() == 2);
    insert_tainted_value(cmp->getOperand(0), v);
    insert_tainted_value(cmp->getOperand(1), v);
    v->visited = true;
  } else if (auto *select = dyn_cast<SelectInst>(v->v)) {
    insert_tainted_value(select->getCondition(), v);
    auto true_val = insert_tainted_value(select->getTrueValue(), v);
    auto false_val = insert_tainted_value(select->getFalseValue(), v);
    if (select->getType()->isPointerTy()) {
      assert(v->ptr_info);
      true_val->ptr_info = v->ptr_info;
      false_val->ptr_info = v->ptr_info;
      v->ptr_info->add_ptr_info_user(true_val);
      v->ptr_info->add_ptr_info_user(false_val);
    }
    v->visited = true;

  } else if (isa<Argument>(v->v)) {
    visit_arg(v);

  } else if (isa<CallBase>(v->v)) {
    visit_call(v);
  } else if (isa<PHINode>(v->v)) {
    visit_phi(v);
  } else if (auto *cast = dyn_cast<CastInst>(v->v)) {
    // cast TO ptr is not allowed
    assert(not cast->getType()->isPointerTy() &&
           "Casting an integer to a ptr is not supported");
    // cast from ptr is allowed (e.g. to check if a ptr is aligned with a
    // modulo operation) as long as it is not casted back into a ptr

    insert_tainted_value(cast->getOperand(0), v);
    v->visited = true;

  } else if (auto *gep = dyn_cast<GetElementPtrInst>(v->v)) {
    visit_gep(v);
    assert(is_tainted(gep->getPointerOperand()));
    v->visited = true;

  } else if (auto *br = dyn_cast<BranchInst>(v->v)) {
    assert(v->getReason() & TaintReason::CONTROL_FLOW);
    v->visited = true;
    if (br->isConditional()) {
      insert_tainted_value(br->getCondition(), v);
    } else {
      // nothing to do
    }
  } else if (auto *sw = dyn_cast<SwitchInst>(v->v)) {
    assert(v->getReason() & TaintReason::CONTROL_FLOW);
    v->visited = true;
    insert_tainted_value(sw->getCondition(), v);
  } else if (auto *resume = dyn_cast<ResumeInst>(v->v)) {
    assert(v->getReason() & TaintReason::CONTROL_FLOW);
    // resume exception: nothing to do just keep it
    insert_tainted_value(resume->getOperand(0), v);
    v->visited = true;
  } else if (auto *ret = dyn_cast<ReturnInst>(v->v)) {
    insert_tainted_value(ret->getOperand(0), v);
    v->visited = true;
  } else if (isa<LandingPadInst>(v->v)) {
    // nothing to do, just keep around
    assert(v->getReason() & TaintReason::CONTROL_FLOW);
    v->visited = true;
  } else if (auto *ext = dyn_cast<ExtractValueInst>(v->v)) {
    insert_tainted_value(ext->getAggregateOperand(), v);
    v->visited = true;
  } else if (auto *ptoi = dyn_cast<PtrToIntInst>(v->v)) {
    // conversion of ptr TO int e.g. for comparison or alignment check is
    // allowed
    insert_tainted_value(ptoi->getPointerOperand(), v);
    v->visited = true;

  } else if (isa<ShuffleVectorInst>(v->v) || isa<ExtractElementInst>(v->v) ||
             isa<InsertElementInst>(v->v)) {
    for (auto *operand : llvm::cast<Instruction>(v->v)->operand_values()) {
      insert_tainted_value(operand, v);
    }
    v->visited = true;
  } else {
    errs() << "Support for analyzing this Value is not implemented yet\n";
    v->v->dump();
    if (auto *inst = dyn_cast<Instruction>(v->v)) {
      errs() << "In: " << inst->getFunction()->getName() << "\n";
      errs() << "Reason:" << v->getReason() << "\n";
    }
    assert(false);
  }

  if (v->is_pointer()) {
    visit_ptr_usages(v);
  }
}

void PrecalculationAnalysis::visit_ptr_usages(
    const std::shared_ptr<TaintedValue> &ptr) {
  assert(ptr->is_pointer());

  if (isa<ConstantPointerNull>(ptr->v)) {
    return;
    // we don't need to trace usages of null to find out if is written or read
  }

  if (auto *global = dyn_cast<GlobalVariable>(ptr->v)) {
    auto *implementation_specifics = ImplementationSpecifics::get_instance();
    if (global == implementation_specifics->COMM_WORLD) {
      // no need to handle all usages of Comm World as we know it is a static
      // object
      return;
    }
    if (is_global_from_std(global)) {
      // don't analyze the usage of std::'s globals in std
      // TODO we could replace for example std::cout
      return;
    }

    if (global->isConstant()) {
      // we don't need to trace the usages of constant ptrs e.g. constant
      // string values, one can just use them in precompute as well
      return;
    }
  }
  if (auto *c = dyn_cast<ConstantExpr>(ptr->v)) {
    auto *as_inst = c->getAsInstruction();
    if (auto *gep = dyn_cast<GetElementPtrInst>(as_inst)) {
      if (is_global_from_std(cast<GlobalValue>(gep->getPointerOperand()))) {
        // constant gep derived from std
        // e.g. a vtable entry
        as_inst->deleteValue();
        return;
      }
    }
    as_inst->deleteValue();
    // don't keep the temporary instruction around
  }

  for (auto *u : ptr->v->users()) {
    if (auto *inst = dyn_cast<Instruction>(u)) {
      if (is_func_from_std(inst->getFunction())) {
        // user may mark functions as "belong to std"
        // we dont analyze those usages- as user told so
        continue;
      }
    }

    if (auto *store = dyn_cast<StoreInst>(u)) {

      // taint the store to analyze if it is important
      auto store_info = insert_tainted_value(store, ptr, false);
      ptr->ptr_info->setIsUsedDirectly(
          true, store_info->ptr_info); // null if stored value is no ptr
      ptr->ptr_info->setIsWrittenTo(store, this);
      auto func = get_function_analysis(store->getFunction());
      func->add_ptr_write(ptr->ptr_info);
      // may need to re-visit if we discovered its importance later
      store_info->visited = false;

      continue;
    }
    if (auto *l = dyn_cast<LoadInst>(u)) {
      if (l->getType()->isPointerTy()) {
        // if a ptr is loaded we need to trace its usages

        // TODO !!!!!
        //  we need better analysis here

        if (ptr->ptr_info->isUsedDirectly() &&
            ptr->ptr_info->getInfoOfDirectUsage()) {
          if (ptr->ptr_info->getInfoOfDirectUsage()->isReadFrom()) {
            insert_tainted_value(l, ptr, false);
          }
        }
      } // else no need to take care about this, reading the val is allowed
      continue;
    }
    if (auto *call = dyn_cast<CallBase>(u)) {
      visit_call_from_ptr(call, ptr);
      continue;
    }
    if (auto *gep = dyn_cast<GetElementPtrInst>(u)) {
      // if gep is relevant
      if (ptr->ptr_info->is_member_relevant(gep)) {
        insert_tainted_value(gep, ptr, false);
      }
      continue;
    }
    if (auto *constant_exp = dyn_cast<ConstantExpr>(u)) {
      auto *as_inst = constant_exp->getAsInstruction();
      auto *gep = dyn_cast<GetElementPtrInst>(as_inst);
      assert(gep &&
             "Constexpr other than GEP for ptr currently not implemented");
      // if gep is relevant
      if (ptr->ptr_info->is_member_relevant(gep)) {
        insert_tainted_value(gep, ptr, false);
      }
      as_inst->deleteValue(); // don't keep temporary instruction
      continue;
    }
    if (isa<ICmpInst>(u)) {
      // nothing to do
      // the compare will be tainted if it is necessary for control flow else
      // we can ignore it
      continue;
    }
    if (auto *phi = dyn_cast<PHINode>(u)) {
      // follow the phi
      insert_tainted_value(phi, ptr, false);
      continue;
    }
    if (auto *select = dyn_cast<SelectInst>(u)) {
      // follow the resulting ptr
      auto select_info = insert_tainted_value(select, ptr, false);
      ptr->ptr_info->merge_with(select_info->ptr_info);

      continue;
    }
    if (auto *ret = dyn_cast<ReturnInst>(u)) {
      visit_ptr_ret(ptr, ret);
      continue;
    }
    if (isa<PtrToIntInst>(u)) {
      // nothing to do cast to int e.g. for comparison is allowed
      continue;
    }

    if (auto *lpad = dyn_cast<LandingPadInst>(u)) {
      bool used_in_catch = false;
      for (unsigned int i = 0; i < lpad->getNumClauses(); ++i) {
        if (lpad->getClause(i) == ptr->v) {
          used_in_catch = true;
          break;
        }
      }
      assert(used_in_catch);
      // nothing to do:
      // we don't care if the typeinfo is used in a catch clause,
      // as the catch itself does not do anything harmful to the ptr
      continue;
    }

    ptr->ptr_info->dump();
    errs() << "Support for analyzing this Value is not implemented yet\n";

    u->dump();

    assert(false);
  }
}

void PrecalculationAnalysis::visit_ptr_ret(
    const std::shared_ptr<TaintedValue> &ptr, llvm::ReturnInst *ret) {
  assert(ret->getOperand(0) == ptr->v);
  // need to merge ptr info for the resulting ptr

  // TODO some duplicate code with
  // visit_arg and taint_all_indirect_call_args
  auto *func = ret->getFunction();
  auto fun_to_precalc = function_analysis.at(func);

  for (auto *call : fun_to_precalc->callsites) {

    auto call_info = insert_tainted_value(call, ptr);
    assert(call_info->ptr_info != nullptr);
    ptr->ptr_info->merge_with(call_info->ptr_info);
  }
}

void PrecalculationAnalysis::insert_function_to_include(llvm::Function *func) {

  auto fun_to_precalc = function_analysis.at(func);
  if (not fun_to_precalc->include_in_precompute) {
    fun_to_precalc->include_in_precompute = true;
    for (auto *call : fun_to_precalc->callsites) {
      auto call_info =
          insert_tainted_value(call, TaintReason::CONTROL_FLOW_CALLEE_NEEDED);
      call_info->visited = false; // may need to re visit if it was later
                                  // discovered that it is important

      if (fun_to_precalc->include_all_callsites) {
        // propagate include_all_callsites
        auto parent = function_analysis.at(call->getFunction());
        parent->include_all_callsites = true;
        insert_function_to_include(call->getFunction());

        include_value_in_precompute(call_info);
      }
    }
  }
}

// TODO: there is the case, where a CALLSITE to user defnied func is in std??
void PrecalculationAnalysis::visit_arg(
    const std::shared_ptr<TaintedValue> &arg_info) {
  auto *arg = cast<Argument>(arg_info->v);
  arg_info->visited = true;

  if (is_func_from_std(arg->getParent())) {
    return;
    // user may mark functions to exclude
  }
  assert(not is_func_from_std(arg->getParent()));

  auto *func = arg->getParent();
  auto fun_to_precalc = function_analysis.at(func);

  if (fun_to_precalc->args_to_use.find(arg->getArgNo()) ==
      fun_to_precalc->args_to_use.end()) {

    // else: nothing to do, this was already visited
    fun_to_precalc->args_to_use.insert(arg->getArgNo());

    for (auto *call : fun_to_precalc->callsites) {
      assert(not is_func_from_std(call->getFunction()));
      auto *operand = call->getArgOperand(arg->getArgNo());
      auto new_val = insert_tainted_value(operand, arg_info);
      new_val->visited =
          false; // may need to re visit if we discover it is important
      if (arg_info->is_pointer()) {
        arg_info->ptr_info->merge_with(new_val->ptr_info);
      }
    }
  }
}

bool PrecalculationAnalysis::is_retval_of_call_needed(
    llvm::CallBase *call) const {
  // check if this is tainted as the ret val is used

  std::set<Value *> users_of_retval;
  std::transform(call->user_begin(), call->user_end(),
                 std::inserter(users_of_retval, users_of_retval.begin()),
                 [](auto *u) { return dyn_cast<Value>(u); });

  for (auto *v : users_of_retval) {
    if (is_tainted(v)) {
      auto taint_info = get_taint_info(v);
      if (taint_info->getReason() !=
          TaintReason::CONTROL_FLOW_ONLY_PRESENCE_NEEDED) {
        return true;
      }
    }
  }
  return false;
}

// we ignore those intrinsics for precompute we dont need to call them
bool should_ignore_intrinsic(Intrinsic::ID id) {

  return
      // intrinsics serving as additional annotations to the IR:
      id == Intrinsic::lifetime_start || id == Intrinsic::lifetime_end ||
      id == Intrinsic::type_test || id == Intrinsic::public_type_test ||
      id == Intrinsic::assume || id == Intrinsic::type_checked_load; // NOLINT
}

// we consider this intrinsics as safe to call during precompute
bool should_call_intrinsic(Intrinsic::ID id) {

  return
      // it is also safe to call ignored intrinsics
      // they only serve as IR annotations anyway
      should_ignore_intrinsic(id) ||
      // std:: functions
      // (https://llvm.org/docs/LangRef.html#standard-c-c-library-intrinsics):
      id == Intrinsic::abs || id == Intrinsic::smax || id == Intrinsic::smin ||
      id == Intrinsic::umax || id == Intrinsic::umin ||
      id == Intrinsic::memcpy || id == Intrinsic::memcpy_inline ||
      id == Intrinsic::memmove || id == Intrinsic::memset ||
      id == Intrinsic::memset_inline || id == Intrinsic::sqrt ||
      id == Intrinsic::powi || id == Intrinsic::sin || id == Intrinsic::cos ||
      id == Intrinsic::pow || id == Intrinsic::exp || id == Intrinsic::exp2 ||
      id == Intrinsic::exp || id == Intrinsic::log || id == Intrinsic::log10 ||
      id == Intrinsic::log2 || id == Intrinsic::fma || id == Intrinsic::fabs ||
      id == Intrinsic::minnum || id == Intrinsic::maxnum ||
      id == Intrinsic::minimum || id == Intrinsic::maximum ||
      id == Intrinsic::copysign || id == Intrinsic::floor ||
      id == Intrinsic::ceil || id == Intrinsic::trunc ||
      id == Intrinsic::rint || id == Intrinsic::nearbyint ||
      id == Intrinsic::round || id == Intrinsic::roundeven ||
      id == Intrinsic::lround || id == Intrinsic::llround ||
      id == Intrinsic::lrint || id == Intrinsic::llrint ||
      // specialized arithmetic
      id == Intrinsic::fmuladd || id == Intrinsic::fma ||
      // exception handling:
      id == Intrinsic::eh_typeid_for ||

      // vector instructions
      Intrinsic::getName(id).starts_with("llvm.x86.sse"); // NOLINT
}

bool PrecalculationAnalysis::is_ptr_usage_in_std_read(
    llvm::CallBase *call, const std::shared_ptr<TaintedValue> &ptr_arg_info) {

  assert(ptr_arg_info->v->getType()->isPointerTy());
  assert(ptr_arg_info->ptr_info);
  assert(call->getCalledFunction()->isIntrinsic() || is_call_to_std(call));
  long arg_no = -1;

  for (unsigned i = 0; i < call->arg_size(); ++i) {
    if (call->getArgOperand(i) == ptr_arg_info->v) {
      arg_no = i;
      break;
    }
  }
  assert(arg_no != -1);

  for (auto *tgt : get_possible_call_targets(call)) {

    if (tgt->isVarArg()) {
      return true; // assume it is
    }

    auto *arg = tgt->getArg(arg_no);
    if (not arg->hasAttribute(llvm::Attribute::ReadNone)) {

      return true;
    }
  }

  return false;
}

bool PrecalculationAnalysis::is_ptr_usage_in_std_write(
    llvm::CallBase *call, const std::shared_ptr<TaintedValue> &ptr_arg_info) {

  assert(ptr_arg_info->v->getType()->isPointerTy());
  assert(ptr_arg_info->ptr_info);
  assert(call->getCalledFunction()->isIntrinsic() || is_call_to_std(call));
  long arg_no = -1;

  for (unsigned i = 0; i < call->arg_size(); ++i) {
    if (call->getArgOperand(i) == ptr_arg_info->v) {
      arg_no = i;
      break;
    }
  }
  assert(arg_no != -1);

  for (auto *tgt : get_possible_call_targets(call)) {
    if (tgt->isVarArg()) {
      return true; // assume it is
    }

    auto *arg = tgt->getArg(arg_no);
    if (not arg->hasAttribute(llvm::Attribute::ReadOnly)) {
      return true;
    }
  }
  return false;
}

void PrecalculationAnalysis::include_call_to_std(
    const std::shared_ptr<TaintedValue> &call_info) {

  assert(isa<CallBase>(call_info->v));
  auto *call = cast<CallBase>(call_info->v);

  for (auto *func : get_possible_call_targets(call)) {
    assert((func->isIntrinsic() &&
            should_call_intrinsic(func->getIntrinsicID())) ||
           is_func_from_std(func));

    // calling into std is safe, as no side effects will occur (other
    // than for the given parameters)
    //  as std is designed to have as fw side effects as possible
    // TODO implement check for exception std::rand and std::cout/cin
    // we just need to make shure all parameters are given

    for (auto &arg : call->args()) {
      auto arg_info = insert_tainted_value(arg, call_info);

      // for ptr parameters: we need to respect if the func
      // reads/writes them
      if (arg->getType()->isPointerTy()) {

        if (is_ptr_usage_in_std_read(call, arg_info)) {
          arg_info->ptr_info->setIsReadFrom(call, this);
          arg_info->ptr_info->setWholePtrIsRelevant(true);
        }
        if (is_ptr_usage_in_std_write(call, arg_info)) {
          arg_info->ptr_info->setIsWrittenTo(call, this);
          arg_info->ptr_info->setWholePtrIsRelevant(true);
        }
        // TODO more like a hotfix? probably overestimating the instructions to
        // be included
        // TODO check if there is another case than operator[] where this is
        // important
        if (call->getType()->isPointerTy()) {
          // if std derives a ptr (e.g. operator[]) we treat it as aliasing to
          // all input ptrs
          call_info->ptr_info->merge_with(arg_info->ptr_info);
        }
      }
      // need all args to be present for the call
      include_value_in_precompute(arg_info);
    }
  }
  include_value_in_precompute(call_info);
}

void PrecalculationAnalysis::visit_call(
    const std::shared_ptr<TaintedValue> &call_info) {
  auto *call = cast<CallBase>(call_info->v);
  assert(!call_info->visited);
  call_info->visited = true;

  std::vector<Function *> possible_targets = get_possible_call_targets(call);

  bool need_return_val = is_retval_of_call_needed(call);
  if (need_return_val) {
    visit_call_for_retval(call_info);
  }

  // we need to check the control flow if an exception is raised
  if (auto *invoke = dyn_cast<InvokeInst>(call)) {
    if (is_invoke_exception_case_needed(invoke) &&
        can_except_in_precompute(invoke)) {
      // if it will not cause an exception, there is no need to have an invoke
      // in this case control flow will not break if we just skip this
      // function, as we know that it does not make the flow go away due to an
      // exception

      visit_invoke_for_exception(call_info);
    }
  }

  if (call->isIndirectCall() && call_info->isIncludeInPrecompute()) {
    // we need to taint the function ptr
    auto func_ptr_info =
        insert_tainted_value(call->getCalledOperand(), call_info);
    func_ptr_info->ptr_info->setIsUsedDirectly(true);
    func_ptr_info->ptr_info->setIsCalled(true);
    include_value_in_precompute(func_ptr_info);
  }

  // analyze if call to str read/writes ptr
  if (is_call_to_std(call)) {
    for (auto &arg : call->args()) {
      if (auto *v = dyn_cast<Value>(&arg)) {
        if (is_tainted(v) && v->getType()->isPointerTy()) {
          if (is_ptr_usage_in_std_write(call, get_taint_info(v))) {
            get_function_analysis(call->getFunction())
                ->add_ptr_write(get_taint_info(v)->ptr_info);
          }
          if (is_ptr_usage_in_std_read(call, get_taint_info(v))) {
            get_function_analysis(call->getFunction())
                ->add_ptr_read(get_taint_info(v)->ptr_info);
          }
        }
      }
    }
  }

  //  check if we need to include the call
  if (not call_info->isIncludeInPrecompute()) {
    if (check_if_call_should_be_included(call_info)) {
      include_value_in_precompute(call_info);
    }
  }
}

bool PrecalculationAnalysis::check_if_call_should_be_included(
    const std::shared_ptr<TaintedValue> &call_info) {
  auto *call = cast<CallBase>(call_info->v);
  for (auto *func : get_possible_call_targets(call)) {
    auto func_analysis = get_function_analysis(func);
    if (func_analysis->include_all_callsites) {
      // set it on parent
      get_function_analysis(call->getFunction())->include_all_callsites = true;
      return true;
    }
    // if it stores an important value
    for (const auto &ptr : func_analysis->getPtrWritten_recursive()) {
      if (is_store_important(call, ptr)) {
        return true;
      }
    }

    if (is_func_from_std(func)) {
      for (auto &arg : call->args()) {
        if (auto *v = dyn_cast<Value>(&arg)) {
          if (is_tainted(v) && v->getType()->isPointerTy()) {
            if (is_ptr_usage_in_std_write(call, get_taint_info(v)) &&
                is_store_important(call, get_taint_info(v)->ptr_info)) {
              include_call_to_std(call_info);
              return true;
            }
          }
        }
      }
    }
  }

  return false;
}

void PrecalculationAnalysis::visit_invoke_for_exception(
    const std::shared_ptr<TaintedValue> &call_info) {

  auto *ivoke = cast<InvokeInst>(call_info->v);
  assert(is_invoke_exception_case_needed(ivoke));
  for (auto *func : get_possible_call_targets(ivoke)) {
    if (not get_function_analysis(func)->can_except_in_precompute) {
      // no exception possible: nothing to do
      continue;
    }
    if (func->isIntrinsic() &&
        should_ignore_intrinsic(func->getIntrinsicID())) {
      // ignore intrinsics
      continue;
    }
    if (is_func_from_std(func)) {
      include_call_to_std(call_info);
      continue;
    }
    if (func->isDeclaration()) {
      ivoke->dump();
      func->dump();
      errs() << "Reason: " << call_info->getReason() << "\n";
      errs() << "In: " << ivoke->getFunction()->getName() << "\n";
    }
    assert(not func->isDeclaration() &&
           "cannot analyze if external function may throw an exception");
    for (auto &bb : *func) {
      if (auto *res = dyn_cast<ResumeInst>(bb.getTerminator())) {
        assert(call_info->isIncludeInPrecompute());
        auto new_val = insert_tainted_value(res, CONTROL_FLOW);
        include_value_in_precompute(new_val);
      }
      for (auto &inst : bb)
        if (auto *cc = dyn_cast<CallBase>(&inst)) {
          if (can_except_in_precompute(cc)) {
            // assert(call_info->isIncludeInPrecompute());
            auto new_val =
                insert_tainted_value(cc, CONTROL_FLOW_EXCEPTION_NEEDED);
            include_value_in_precompute(new_val);
          }
        }
    }
  }
}

void PrecalculationAnalysis::visit_call_for_retval(
    const std::shared_ptr<TaintedValue> &call_info) {

  auto *call = cast<CallBase>(call_info->v);
  assert(is_retval_of_call_needed(call));

  call_info->addReason(CONTROL_FLOW_RETURN_VALUE_NEEDED);
  if (is_allocation(call)) {
    // nothing to do, just keep this call around, it will later be replaced
    for (auto &arg : call->args()) {
      auto arg_info = insert_tainted_value(arg, call_info);
    }
  } else if (not call->isIndirectCall() &&
             call->getCalledFunction()->isIntrinsic() &&
             should_call_intrinsic(
                 call->getCalledFunction()->getIntrinsicID())) {
    if (not should_ignore_intrinsic(
            call->getCalledFunction()->getIntrinsicID())) {
      // consider it same as call to std
      include_call_to_std(call_info);
    }
  } else if (is_call_to_std(call)) {
    include_call_to_std(call_info);
  } else {
    for (auto *func : get_possible_call_targets(call)) {
      if (func->isDeclaration()) {
        errs() << "\n";
        call->dump();
        func->dump();
        errs() << "In: " << call->getFunction()->getName() << " intrinsic?"
               << func->isIntrinsic() << "\n";
      }
      assert(not func->isDeclaration() &&
             "cannot analyze if calling external function for return value "
             "has "
             "side effects");
      for (auto &bb : *func) {
        if (auto *ret = dyn_cast<ReturnInst>(bb.getTerminator())) {
          insert_tainted_value(ret, call_info);
        }
      }
    }
  }
}

void PrecalculationAnalysis::visit_call_from_ptr(
    llvm::CallBase *call, const std::shared_ptr<TaintedValue> &ptr) {

  std::set<unsigned int> ptr_given_as_arg;
  for (unsigned int i = 0; i < call->arg_size(); ++i) {
    if (call->getArgOperand(i) == ptr->v) {
      ptr_given_as_arg.insert(i);
    }
  }

  if (call->getCalledOperand() == ptr->v) {
    // visit from the function ptr: nothing to check
    assert(ptr_given_as_arg.empty() && "Function ptr getting itself as an "
                                       "argument is currently not supported");
    return;
  }

  // if (not is_store_important(call,ptr->ptr_info)){
  //  no need to analyze it, if nothing is done with the ptr afterward anyway
  //  but this func does currently not take into account the GEP members of ptr
  //    return;
  //  }

  auto *func = call->getCalledFunction();
  assert(not ptr_given_as_arg.empty());
  assert(ptr->ptr_info);

  // errs() << "Visit\n";
  // call->dump();

  if (not call->isIndirectCall()) {
    if (func == mpi_func->mpi_send || func == mpi_func->mpi_Isend ||
        func == mpi_func->mpi_recv || func == mpi_func->mpi_Irecv) {
      assert(ptr_given_as_arg.size() == 1);
      if (*ptr_given_as_arg.begin() == 0) {
        ptr->v->dump();
        call->dump();
        assert(false &&
               "Tracking Communication to get the envelope is currently "
               "not supported");
      } else {
        // we know that the other arguments are not important e.g. not written
        // to like if the communicator is used
        return;
      }
    }
    if (not call->isIndirectCall() && is_free(call)) {
      // the precompute library will take care of free, so no need to taint it
      return;
    }

    if (func == mpi_func->mpi_comm_size || func == mpi_func->mpi_comm_rank) {
      // we know it is safe to execute these "readonly" funcs
      if (*ptr_given_as_arg.begin() == 0 && ptr_given_as_arg.size() == 1) {
        // nothing to: do only reads the communicator
        // ptr is the communicator
      } else {
        // the needed value is the result of reading the comm
        assert(*ptr_given_as_arg.begin() == 1 && ptr_given_as_arg.size() == 1);
        // treat it like a store to ptr:
        // value is only necessary it ptr is read
        ptr->ptr_info->setIsWrittenTo(call, this);

        if (is_store_important(call, ptr->ptr_info)) {
          auto call_info = insert_tainted_value(call, ptr, false);
          auto comm_info =
              insert_tainted_value(call->getArgOperand(0),
                                   ptr); // we also need to keep the comm
          include_value_in_precompute(ptr);
          include_value_in_precompute(call_info);
          include_value_in_precompute(comm_info);
        }
      }
      return;
    }
    if (func == mpi_func->mpi_init || func == mpi_func->mpi_init_thread) {
      // skip: MPI_Init will only transfer the cmd line args to all processes,
      // not modify them otherwise
      return;
    }
    if (func == mpi_func->mpi_send_init || func == mpi_func->mpi_recv_init) {
      // skip: these functions will be managed separately anyway
      // it may be the case, that e.g. the buffer or request aliases with
      // something important
      return;
    }

    if (is_mpi_function(func)) {
      // TODO is there anything else in MPI we need to handle special??
      // call->dump();
      // errs() << "In: " << call->getFunction()->getName() << "\n";
      assert(not is_included_in_precompute(call));
      return;
    }

    if (is_allocation(func)) {
      // skip: alloc needs to be handled differently
      // but needs to be tainted so it will be replaced later
      auto call_info = insert_tainted_value(call, ptr, false);

      assert(false && "a ptr given into an allocation call???");
      return;
    }

    if (func->isIntrinsic() &&
        should_ignore_intrinsic(func->getIntrinsicID())) {
      // skip
      return;
    }

    if ((func->isIntrinsic() &&
         should_call_intrinsic(func->getIntrinsicID())) ||
        is_func_from_std(func)) {

      if (is_ptr_usage_in_std_write(call, ptr)) {
        ptr->ptr_info->setIsWrittenTo(call, this);
        if (is_store_important(call, ptr->ptr_info)) {
          auto call_info = insert_tainted_value(call, ptr, false);
          include_call_to_std(call_info);
          assert(ptr->ptr_info->isWrittenTo());
        }
      }
      return;
    }
  }

  for (auto *func : get_possible_call_targets(call)) {
    for (auto arg_num : ptr_given_as_arg) {
      auto *arg = func->getArg(arg_num);
      if (arg->hasAttribute(Attribute::NoCapture) &&
          arg->hasAttribute(Attribute::ReadOnly)) {
        continue; // nothing to do: reading the val is allowed
        // TODO has foo( int ** array){ array[0][0]=0;} also readonly? as
        // first ptr lvl is only read
      }
      if (func->isDeclaration()) {
        errs() << "Can not analyze usage of external function:\n";
        ptr->v->dump();
        call->dump();
        errs() << "In: " << call->getFunction()->getName() << "\n";
        assert(false);
      } else {
        auto call_info = insert_tainted_value(call, ptr, false);
        auto new_val = insert_tainted_value(arg, ptr, false);
        ptr->ptr_info->merge_with(new_val->ptr_info);
        assert(new_val->ptr_info == ptr->ptr_info);
      }
    }
  }
}

void PrecalculationAnalysis::include_value_in_precompute(
    const std::shared_ptr<TaintedValue> &taint_info) {

  if (not taint_info->isIncludeInPrecompute()) {
    taint_info->setIncludeInPrecompute();

    if (auto *cc = dyn_cast<StoreInst>(taint_info->v)) {
      assert(is_store_important(
          cc, get_taint_info(cc->getPointerOperand())->ptr_info));
      if (cc->getPointerOperand()->getName() == "NBodies") {
        // TODO
        // DEBUG ONLY
        /*
        errs() << "Where is inclusion happening?\n";
        errs() << to_string(boost::stacktrace::stacktrace());
        assert(is_tainted(cc->getPointerOperand()));

        assert(is_store_important(cc,
        get_taint_info(cc->getPointerOperand())->ptr_info));

        assert(false);
         */
      }
    }

    insert_necessary_control_flow(taint_info->v);
    for (const auto &p : taint_info->needs) {
      include_value_in_precompute(p);
    }
  } // else: already included, nothing to do
}

std::shared_ptr<TaintedValue>
PrecalculationAnalysis::insert_tainted_value(llvm::Value *v,
                                             TaintReason reason) {

  std::shared_ptr<TaintedValue> inserted_elem = nullptr;

  if (not is_tainted(v)) {
    // only if not already in set
    inserted_elem = std::make_shared<TaintedValue>(v);
    tainted_values.insert(inserted_elem);
    if (v->getType()->isPointerTy()) {
      // create empty info
      inserted_elem->ptr_info = std::make_shared<PtrUsageInfo>(inserted_elem);
    }

  } else {
    // the present value form the set
    inserted_elem = *std::find_if(tainted_values.begin(), tainted_values.end(),
                                  [&v](const auto &vv) { return vv->v == v; });
  }
  inserted_elem->addReason(reason);

  assert(inserted_elem != nullptr);
  return inserted_elem;
}

std::shared_ptr<TaintedValue> PrecalculationAnalysis::insert_tainted_value(
    llvm::Value *v, const std::shared_ptr<TaintedValue> &from,
    bool needed_from) {
  std::shared_ptr<TaintedValue> inserted_elem = nullptr;

  if (not is_tainted(v)) {
    // only if not already in set

    if (auto *inst = dyn_cast<Instruction>(v)) {
      // don't analyze std::s internals
      if (is_func_from_std(inst->getFunction())) {
        // inst->getFunction()->dump();
        inst->dump();

        errs() << "In: " << inst->getFunction()->getName() << "\n";
        errs() << "from:";
        from->v->dump();
        if (auto *ii = dyn_cast<Instruction>(from->v)) {
          errs() << "In: " << ii->getFunction()->getName() << "\n";
        }
      }
      assert(not is_func_from_std(inst->getFunction()));
    }
    /*
    if (auto *arg = dyn_cast<Argument>(v)) {
      if (is_func_from_std(arg->getParent())) {
        arg->dump();

        errs() << "In: " << arg->getParent()->getName() << "\n";
        errs() << "from:";
        from->v->dump();
      }
      assert(not is_func_from_std(arg->getParent()));
    }*/
    // TODO user may mark funcs that are presendt as "same as std"

    inserted_elem = std::make_shared<TaintedValue>(v);
    auto pair = tainted_values.insert(inserted_elem);
    assert(pair.second); // assert it is newly inserted

    if (v->getType()->isPointerTy()) {
      // create empty info
      inserted_elem->ptr_info = std::make_shared<PtrUsageInfo>(inserted_elem);
    }
    if (from != nullptr) {
      // we don't care why the Control flow was tagged for the parent
      inserted_elem->addReason(from->getReason() &
                               TaintReason::REASONS_TO_PROPERGATE);
      if (needed_from) {
        inserted_elem->needed_for.insert(from);
        from->needs.insert(inserted_elem);
        if (from->isIncludeInPrecompute()) {
          include_value_in_precompute(inserted_elem);
        }
      } else {
        from->needed_for.insert(inserted_elem);
        inserted_elem->needs.insert(from);
      }
    }
  } else { // already present
    // the present value form the set
    inserted_elem = *std::find_if(tainted_values.begin(), tainted_values.end(),
                                  [&v](const auto &vv) { return vv->v == v; });
    if (from != nullptr && needed_from) {
      auto pair = inserted_elem->needed_for.insert(from);
      from->needs.insert(inserted_elem);
      if (pair.second) // was inserted
      {
        // may need to re-visit if we discover that we need it later
        inserted_elem->visited = false;
      }
      // we don't care why the Control flow was tagged for te parent
      inserted_elem->addReason(from->getReason() &
                               TaintReason::REASONS_TO_PROPERGATE);
      if (from->isIncludeInPrecompute()) {
        include_value_in_precompute(inserted_elem);
      }
    }
    if (from != nullptr && not needed_from) {
      from->needed_for.insert(inserted_elem);
      auto pair = inserted_elem->needs.insert(from);
      // we don't care why the Control flow was tagged for te parent
      inserted_elem->addReason(from->getReason() &
                               TaintReason::REASONS_TO_PROPERGATE);

      if (pair.second) // was inserted
      {
        // may need to re-visit if we discover that we need it later
        inserted_elem->visited = false;
      }
    }
  }

  // this code is asserting that we will visit the call if the retval is needed
#ifndef NDEBUG
  if (auto *cc = dyn_cast<CallBase>(v)) {
    if (is_retval_of_call_needed(cc) && not cc->isIndirectCall()) {
      auto *func = cc->getCalledFunction();
      // TODO proper management why this is not working as is
      // TODO this is only a hotfix
      if (not(get_function_analysis(func)->include_in_precompute ||
              // part of the special functions which content is not analyzed
              is_func_from_std(func) || func->isIntrinsic() ||
              is_mpi_function(func))) {
        inserted_elem->visited = false;
      }

      // cc->dump();
      if (not(get_function_analysis(func)->include_in_precompute ||
              // part of the special functions which content is not analyzed
              is_func_from_std(func) || func->isIntrinsic() ||
              is_mpi_function(func) ||
              // or we need to visit it in order to analyze the function
              !inserted_elem->visited)) {
        // at least one user must be open to be visited in order to discover
        // that this call is important
        bool all_retval_users_visited = true;
        for (auto *u : cc->users()) {
          if (auto *ii = dyn_cast<Instruction>(u)) {
            if (is_tainted(ii)) {

              if (not get_taint_info(ii)->visited ||
                  get_taint_info(ii)->getReason() ==
                      TaintReason::CONTROL_FLOW_ONLY_PRESENCE_NEEDED) {
                // if the other is only tainted for its presence it does not use
                // the retval
                all_retval_users_visited = false;
              }
            }
          }
        }
        assert(not all_retval_users_visited);
      }
    }
  }
#endif

  assert(inserted_elem != nullptr);
  return inserted_elem;
}

void PrecalculationAnalysis::insert_necessary_control_flow(Value *v) {
  if (auto *inst = dyn_cast<Instruction>(v)) {
    auto *bb = inst->getParent();
    if (not bb->isEntryBlock()) {
      // we need to insert the instruction that lets the control flow go
      // here
      for (auto *pred_bb : predecessors(bb)) {
        auto *term = pred_bb->getTerminator();

        if (auto *invoke = dyn_cast<InvokeInst>(term)) {
          if (invoke->getUnwindDest() == bb &&
              can_except_in_precompute(invoke)) {
            auto new_val =
                insert_tainted_value(term, TaintReason::CONTROL_FLOW);
            new_val->addReason(TaintReason::CONTROL_FLOW_EXCEPTION_NEEDED);
            // it may need to be re-visited if we find out that we do need
            // the exception path
            new_val->visited = false;
            include_value_in_precompute(new_val);
          } else {
            if (invoke->getUnwindDest() == bb) {
              // this exception block cannot be visited in precompute anyway
              continue;
            } else {
              assert(invoke->getNormalDest() == bb);
              auto new_val =
                  insert_tainted_value(term, TaintReason::CONTROL_FLOW);
              if (not can_except_in_precompute(invoke)) {
                new_val->addReason(
                    TaintReason::CONTROL_FLOW_ONLY_PRESENCE_NEEDED);
                // only the normal dest is needed
                // NOT include_value_in_precompute(new_val);
                // we dont need the invoke to check if exception is thrown as we
                // know no meaningful exception can be thrown
              } else {
                // can except
                // include_value_in_precompute(new_val);
                new_val->addReason(TaintReason::CONTROL_FLOW_EXCEPTION_NEEDED);
                // if the exception Part is not tainted: remove and ignore any
                // exception
              }
            }
          }
        } else {
          auto new_val = insert_tainted_value(term, TaintReason::CONTROL_FLOW);
          include_value_in_precompute(new_val);
        }
      }
    } else {
      // BB is function entry block
      insert_function_to_include(bb->getParent());
    }
  }
}

std::vector<llvm::Function *>
PrecalculationAnalysis::get_possible_call_targets(llvm::CallBase *call) const {
  std::vector<llvm::Function *> possible_targets;
  if (call->isIndirectCall()) {
    possible_targets = virtual_call_sites.get_possible_call_targets(call);
  } else {
    possible_targets.push_back(call->getCalledFunction());
    return possible_targets;
  }

  if (is_func_from_std(call->getFunction())) {
    // we dont need to analyze the sts::'s internals
    // if std:: calls a user function indirectly it will get a ptr to it
    // anyway
    return possible_targets;
  }

  if (possible_targets.empty()) {
    // can call any function with same type that we get a ptr of somewhere
    for (const auto &pair : function_analysis) {
      auto func = pair.second;
      if (func->is_func_ptr_captured) {
        if (func->func->getFunctionType() == call->getFunctionType())
          possible_targets.push_back(func->func);
      }
    }
    // TODO can we check that we will not be able to get a ptr to a function
    // outside of the module?
  }

  if (possible_targets.empty()) {
    call->dump();
    errs() << "In: " << call->getFunction()->getName() << "\n";
  }
  assert(not possible_targets.empty() && "could not find tgts of call");

  /*
  for (auto *tgt : possible_targets) {
    if ( call->isIndirectCall() && is_func_from_std(tgt)) {
      call->dump();
      errs() << "In: " << call->getFunction()->getName() << "\n";
      errs() << "Indirect calls to std are not supported\n";
      assert(false);
    }
  }*/
  return possible_targets;
}

void PrecalculationAnalysis::print_analysis_result_remarks() {

  for (const auto &v : tainted_values) {
    if (auto *inst = dyn_cast<Instruction>(v->v)) {
      errs() << "need for reason: " << v->getReason() << "\n";
      errs() << inst->getFunction()->getName() << "\n";
      inst->dump();
    }
  }
  // debug_printings();
}

// TODO: move to debug file?
void PrecalculationAnalysis::debug_printings() {
  errs() << "ADDITIONAL DEBUG PRINTING\n";

  std::set<std::shared_ptr<PtrUsageInfo>> dumped;

  for (const auto &v : tainted_values) {
    if (v->v->getName() == "this") {
      auto pair = dumped.insert(v->ptr_info);
      if (pair.second) {
        errs() << "THIS PTR\n";
        v->ptr_info->dump();
      }
    }
  }
  // assert(false);
}
std::set<std::shared_ptr<PrecalculationFunctionAnalysis>>
PrecalculationAnalysis::getFunctionsToInclude() const {
  std::set<std::shared_ptr<PrecalculationFunctionAnalysis>> result;
  for (const auto &pair : function_analysis) {
    if (pair.second->include_in_precompute) {
      result.insert(pair.second);
    }
  }
  return result;
}
Function *PrecalculationAnalysis::getEntryPoint() const { return entry_point; }
const std::vector<llvm::CallBase *> &

PrecalculationAnalysis::getToReplaceWithEnvelopeRegister() const {
  return to_replace_with_envelope_register;
}

// removes all template args from the given name
std::string get_name_without_templates(const std::string &demangled_name) {

  auto pos = demangled_name.begin();
  auto end_pos = pos;
  auto start_pos = pos;

  std::string result = "";
  int template_nesting_level = 0;

  while (pos != demangled_name.end()) {

    if (*pos == '<') {
      if (template_nesting_level == 0) {
        start_pos = pos;
        // directly use std::copy?
        result += std::string(end_pos, pos);
      }
      template_nesting_level++;
    }
    // if template_nesting_level==0 whe are looking at operator> or >>
    if (*pos == '>' && template_nesting_level > 0) {
      template_nesting_level--;
      if (template_nesting_level == 0) {
        end_pos = pos + 1;
      }
    }

    ++pos;
    // special case for operators << or <
    if (pos == demangled_name.end() && template_nesting_level != 0) {
      // ignore the first < and re-start processing from there
      template_nesting_level = 0;
      pos = start_pos + 1;
    }
  }

  assert(template_nesting_level == 0);
  result = result + std::string(end_pos, pos);

  return result;
}

// only gets the name of a function if a demangled name contains a return
// param or template args
std::string get_function_name(const std::string &demangled_name) {

  auto no_template = get_name_without_templates(demangled_name);

  std::istringstream iss(no_template);
  std::string item;
  std::vector<std::string> elems;
  while (std::getline(iss, item, ' ')) {
    elems.push_back(item);
  }

  if (elems.size() > 1) {
    // find the element with the '('
    for (size_t i = 0; i < elems.size(); ++i) {
      if (elems[i].rfind('(') != std::string ::npos) {
        if (i != 0 && elems[i - 1].rfind("operator") != std::string ::npos) {
          return elems[i - 1] + " " + elems[i];
        } else {
          return elems[i];
        }
      }
    }
  }

  return no_template;
}

// returns true if func is from std or part of the
// COMPILER_ASSISTED_MATCHING_ALLOW_EXTERNAL_FUNCTIONS environment variable
bool PrecalculationAnalysis::is_func_from_std(llvm::Function *func) const {

  assert(func);

  // C API
  llvm::LibFunc lib_func;
  bool in_lib = analysis_results->getTLI()->getLibFunc(*func, lib_func);
  if (in_lib) {
    return true;
  }

  auto demangled_fname =
      get_function_name(llvm::demangle(func->getName().str()));

  // errs() << "Test if in std:\n" << func->getName() <<demangled_fname <<
  // "\n";

  for (auto prefix : allowed_function_prefixes) {
    if (demangled_fname.rfind(prefix, 0) == 0) {
      return true;
    }
  }

  // more like a stack ptr than a function call
  if (func->getName() == "__errno_location") {
    return true;
  }

  if (func->getName() == "__cxa_throw") {
    return true;
  }

  if (func->getName() == "__cxa_allocate_exception") {
    // TODO is allocator
    return true;
  }
  if (func->getName() == "__cxa_free_exception") {
    // TODO is free
    return true;
  }
  if (func->getName() == "__cxa_begin_catch") {
    // TODO is free
    return true;
  }

  // TODO why it is not in TLI info??
  if (func->getName() == "rand") {
    // calling rand in precompute is actually "safe",
    // as one should usa a random seed anyway it doesn't matter if we call
    // it in precompute
    return true;
  }

  return false;
}

bool PrecalculationAnalysis::can_except_in_precompute(
    llvm::CallBase *call) const {

  if (is_interaction_with_cout(call)) {
    return false;
  }

  for (auto *f : get_possible_call_targets(call)) {
    if (function_analysis.at(f)->can_except_in_precompute) {
      return true;
    }
  }
  return false;
}

bool PrecalculationAnalysis::is_store_important(
    llvm::Instruction *inst, const std::shared_ptr<PtrUsageInfo> &ptr_info) {
  if (isa<StoreInst>(inst)) {
    return is_store_important(cast<StoreInst>(inst), ptr_info);
  }
  if (isa<CallBase>(inst)) {
    return is_store_important(cast<CallBase>(inst), ptr_info);
  }
  assert(false && "Not a store instruction");
}

bool PrecalculationAnalysis::is_store_important(
    llvm::StoreInst *store, const std::shared_ptr<PtrUsageInfo> &ptr_info) {

  // the ptr must be used
  assert(std::find_if(ptr_info->getPtrsWithThisInfo().begin(),
                      ptr_info->getPtrsWithThisInfo().end(),
                      [&store](auto elem) {
                        if (auto elem_instance = elem.lock()) {
                          return elem_instance->v == store->getPointerOperand();
                        }
                        return false;
                      }) != ptr_info->getPtrsWithThisInfo().end());
  if (not ptr_info->isReadFrom()) {
    return false;
  }

  if (store_happens_after_all_loads(store, ptr_info)) {
    return false;
  }

  return true;
}

bool PrecalculationAnalysis::is_store_important(
    llvm::CallBase *call, const std::shared_ptr<PtrUsageInfo> &ptr_info) {

  // TODO can we add a the ptr must be used assertion?

  if (not ptr_info->isReadFrom()) {
    return false;
  }
  if (store_happens_after_all_loads(call, ptr_info)) {
    return false;
  }

  return true;
}

// TODO better function name
//  if an instruction in foo is included in the set: the resulting set will also
//  include all calls to foo
void PrecalculationAnalysis::get_all_transitive_insts(
    std::set<llvm::Instruction *> &instrs) {

  std::set<llvm::Function *> visited;

  // so that iterator over original set does not get broken
  std::set<Instruction *> to_insert;

  bool grown = true;

  while (grown) {
    grown = false;
    for (auto *inst : instrs) {
      auto pair = visited.insert(inst->getFunction());
      if (pair.second) { // newly inserted
        auto func = get_function_analysis(inst->getFunction());
        for (auto *cc : func->callsites) {
          to_insert.insert(cc);
        }
      }
    }
    for (auto *ii : to_insert) {
      auto pair = instrs.insert(ii);
      grown = grown | pair.second;
    }
  }
}

bool PrecalculationAnalysis::store_happens_after_all_loads(
    llvm::Instruction *inst, const std::shared_ptr<PtrUsageInfo> &ptr_info) {

  auto loads = ptr_info->getLoads();
  get_all_transitive_insts(loads);

  std::set<Instruction *> stores;
  stores.insert(inst);
  get_all_transitive_insts(stores);

  for (auto *l : loads) {
    for (auto *s : stores) {
      // TODO include all destructors for debugging only
      if (l->getFunction() == s->getFunction() && l != s) {
        // if load and store are the same (aka call to func that loads and
        // stores): no need to do something, either the ptr usage is not needed,
        // or it will be loaded afterward (then it is needed)
        auto *domtree = analysis_results->getDomTree(*l->getFunction());
        // TODO efficiency: provide LoopInfo?
        if (llvm::isPotentiallyReachable(s, l, nullptr, domtree, nullptr)) {
          return false;
        }
      }
    }
  }
  return true;
}
