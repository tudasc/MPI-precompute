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

#include <random>
#include <regex>

#include "conflict_detection.h"
#include "devirt_analysis.h"
#include "implementation_specific.h"
#include "mpi_functions.h"
#include "precalculation.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"

#include "llvm/Demangle/Demangle.h"

#include "Precompute_insertion.h"

#include "debug.h"
using namespace llvm;

// for more scrutiny under testing:
// the order of visiting the values should make no difference
// #define SHUFFLE_VALUES_FOR_TESTING

void print_childrens(const std::shared_ptr<TaintedValue> &parent,
                     unsigned int indent = 0) {
  if (indent > 10 || parent == nullptr || parent->v == nullptr)
    return;

  for (unsigned int i = 0; i < indent; ++i) {
    errs() << "\t";
  }
  parent->v->dump();
  for (const auto &c : parent->children) {
    print_childrens(c, indent + 1);
  }
}

void print_parents(const std::shared_ptr<TaintedValue> &child,
                   unsigned int indent = 0) {
  if (indent > 2 || child == nullptr || child->v == nullptr)
    return;

  for (unsigned int i = 0; i < indent; ++i) {
    errs() << "\t";
  }
  child->v->dump();
  for (const auto &p : child->parents) {
    print_parents(p, indent + 1);
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

  unwindBB->dump();
  for (auto &i : *unwindBB) {
    i.dump();
    errs() << "Tainted? " << is_tainted(&i) << "\n";
  }

  auto *func = invoke->getCalledFunction();
  if (func->hasFnAttribute(Attribute::AttrKind::NoUnwind)) {
    return false;
  }
  // may yield an exception
  return true;
}

// True if callee needs to be called as it contains tainted instructions
// or True if callee can raise an exception and the exception handling code is
// actually tainted if exception handling is not tainted, we dont need to handle
// the exception anyway and abortion is fine in this case
bool PrecalculationAnalysis::is_invoke_necessary_for_control_flow(
    llvm::InvokeInst *invoke) const {

  assert(invoke);

  if (invoke->isIndirectCall()) {
    // TODO if we know by devirt analysis that call is not needed we can exclude
    // it
    return true;
  }
  // calling into something we need
  if (std::find_if(functions_to_include.begin(), functions_to_include.end(),
                   [&invoke](auto f) {
                     return f->func == invoke->getCalledFunction();
                   }) != functions_to_include.end()) {
    return true;
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

// is function known to not throw an exception during precompute
// if one of those throws: the whole precompute has to abort anyway
bool is_func_known_to_be_safe(llvm::Function *func) {
  if (is_allocation(func) || func == mpi_func->mpi_comm_size ||
      func == mpi_func->mpi_comm_rank ||
      // if barrier in precompute: all ranks can call them (if not all ranks
      // arrive at this point programm will deadlock anyway)
      func == mpi_func->mpi_barrier ||
      func ==
          mpi_func->mpi_finalize || // will not "except" but end the programm
      func->getName() ==
          "MPI_Abort" // calling would also abort main program, calling it
                      // during precompute is "safe" in that regard it will
                      // abort, not "except"
  ) {
    return true;
  }

  if (func->hasFnAttribute(llvm::Attribute::NoUnwind)) {
    // no exception possible
    return true;
  }

  if (func->isDeclaration()) {
    // dont know, need to assume it can throw
    return false;
  }

  // TODO one needs to go through every callee and check if it is safe as well
  return false;
}

void PrecalculationAnalysis::add_precalculations(
    const std::vector<llvm::CallBase *> &to_precompute) {
  to_replace_with_envelope_register = to_precompute;

  for (auto *call : to_precompute) {
    bool is_send = call->getCalledFunction() == mpi_func->mpi_send_init;
    auto *tag = get_tag_value(call, is_send);
    auto *src = get_src_value(call, is_send);

    insert_tainted_value(tag, TaintReason::COMPUTE_TAG);
    insert_tainted_value(src, TaintReason::COMPUTE_DEST);
    // TODO precompute comm as well?
    auto new_val = insert_tainted_value(call, TaintReason::CONTROL_FLOW);
    new_val->visited = true;
  }

  find_all_tainted_vals();

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

  // if tags or control flow depend on argc or argv MPI_Init will be tainted (as
  // it writes argc and argv)

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

  // only for asserting that the subset is valid:
  std::set<BasicBlock *> tainted_blocks;
  // taint all the blocks that are relevant
  std::transform(tainted_values.begin(), tainted_values.end(),
                 std::inserter(tainted_blocks, tainted_blocks.begin()),
                 [](auto v) -> BasicBlock * {
                   if (auto *inst = dyn_cast<Instruction>(v->v))
                     return inst->getParent();
                   else
                     return nullptr;
                 });
  tainted_blocks.erase(nullptr);
  // assert CFG is valid
  for (auto *bb : tainted_blocks) {
    for (auto pred = pred_begin(bb); pred != pred_end(bb); ++pred) {
      assert(tainted_blocks.find(*pred) != tainted_blocks.end());
    }
  }
}

void PrecalculationAnalysis::visit_load(
    const std::shared_ptr<TaintedValue> &load_info) {
  assert(not load_info->visited);
  load_info->visited = true;
  auto *load = dyn_cast<LoadInst>(load_info->v);
  assert(load);

  auto loaded_from = insert_tainted_value(load->getPointerOperand(), load_info);
  assert(loaded_from->is_pointer());

  if (loaded_from->ptr_info == nullptr) {
    loaded_from->ptr_info = std::make_shared<PtrUsageInfo>(loaded_from);
  }
  assert(loaded_from->ptr_info);
  loaded_from->ptr_info->setIsReadFrom(true);

  if (load_info->is_pointer()) {
    loaded_from->ptr_info->setIsUsedDirectly(true, load_info->ptr_info);
  } else {
    loaded_from->ptr_info->setIsUsedDirectly(true);
  }
}

void PrecalculationAnalysis::visit_store_from_value(
    const std::shared_ptr<TaintedValue> &store_info) {
  auto *store = dyn_cast<StoreInst>(store_info->v);
  assert(store);

  assert(is_tainted(store->getValueOperand()));
  auto new_val = insert_tainted_value(store->getPointerOperand(), store_info);
  if (not new_val->ptr_info) {
    new_val->ptr_info = std::make_shared<PtrUsageInfo>(store_info);
  }
  new_val->ptr_info->setIsWrittenTo(true);
  if (store->getValueOperand()->getType()->isPointerTy()) {
    auto val_info =
        insert_tainted_value(store->getValueOperand(),
                             nullptr); // will just do a finding of the value
    new_val->ptr_info->setIsUsedDirectly(true, val_info->ptr_info);
  } else {
    new_val->ptr_info->setIsUsedDirectly(true);
  }
}

void PrecalculationAnalysis::visit_store_from_ptr(
    const std::shared_ptr<TaintedValue> &store_info) {
  auto *store = dyn_cast<StoreInst>(store_info->v);
  assert(store);

  assert(is_tainted(store->getPointerOperand()));
  // does only the find:
  auto ptr = insert_tainted_value(store->getPointerOperand(), store_info);
  ptr->ptr_info->setIsUsedDirectly(
      true, store_info->ptr_info); // null if stored value is no ptr
  ptr->ptr_info->setIsWrittenTo(true);
  // we only need the stored value if it is used later
  if (ptr->ptr_info->isReadFrom()) {
    auto new_val = insert_tainted_value(store->getValueOperand(), store_info);
  }
}

void PrecalculationAnalysis::visit_store(
    const std::shared_ptr<TaintedValue> &store_info) {
  assert(not store_info->visited);
  store_info->visited = true;
  auto *store = dyn_cast<StoreInst>(store_info->v);
  assert(store);

  if (is_tainted(store->getPointerOperand())) {
    visit_store_from_ptr(store_info);
  } else {
    assert(is_tainted(store->getValueOperand()));
    visit_store_from_value(store_info);
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
  errs() << "Visit\n";
  v->v->dump();

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
    // cast from ptr is allowed (e.g. to check if a ptr is aligned with a modulo
    // operation) as long as it is not casted back into a ptr

    insert_tainted_value(cast->getOperand(0), v);
    v->visited = true;

  } else if (auto *gep = dyn_cast<GetElementPtrInst>(v->v)) {
    visit_gep(v);
    assert(is_tainted(gep->getPointerOperand()));
    v->visited = true;

  } else if (auto *br = dyn_cast<BranchInst>(v->v)) {
    assert(v->getReason() == TaintReason::CONTROL_FLOW);
    v->visited = true;
    if (br->isConditional()) {
      insert_tainted_value(br->getCondition(), v);
    } else {
      // nothing to do
    }
  } else if (auto *sw = dyn_cast<SwitchInst>(v->v)) {
    assert(v->getReason() == TaintReason::CONTROL_FLOW);
    v->visited = true;
    insert_tainted_value(sw->getCondition(), v);
  } else if (auto *resume = dyn_cast<ResumeInst>(v->v)) {
    assert(v->getReason() == TaintReason::CONTROL_FLOW);
    // resume exception: nothing to do just keep it
    insert_tainted_value(resume->getOperand(0), v);
    v->visited = true;
  } else if (auto *ret = dyn_cast<ReturnInst>(v->v)) {
    insert_tainted_value(ret->getOperand(0), v);
    v->visited = true;
  } else if (isa<LandingPadInst>(v->v)) {
    // nothing to do, just keep around
    assert(v->getReason() == TaintReason::CONTROL_FLOW);
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

  if (auto *global = dyn_cast<GlobalVariable>(ptr->v)) {
    auto *implementation_specifics = ImplementationSpecifics::get_instance();
    if (global == implementation_specifics->COMM_WORLD) {
      // no need to handle all usages of Comm World as we know it is a static
      // object
      return;
    }
  }

  if (isa<ConstantPointerNull>(ptr->v)) {
    return;
    // we dont need to trace usages of null to find out if if is written or read
  }

  if (ptr->ptr_info == nullptr) {
    // this pointer is not needed
    // if this pointer is indeed needed it will later be visited again if the
    // ptr info was initialized
    return;
  }

  if (not ptr->ptr_info->isReadFrom()) {
    // this pointers CONTENT (the pointee) is currently not needed
    // if it is needed later it will be visited again
    // if only the ptr value is needed: no need to keep track of its content
    return;
  }

  for (auto *u : ptr->v->users()) {
    if (auto *s = dyn_cast<StoreInst>(u)) {
      // if we dont read the ptr directly, we dont need to capture the stores
      //  e.g. a struct ptr where the first member is not used
      if (s->getPointerOperand() == ptr->v) {
        // store to this ptr
        if ((ptr->ptr_info->isUsedDirectly() ||
             ptr->ptr_info->isWholePtrIsRelevant()) &&
            ptr->ptr_info->isReadFrom()) {
          auto new_val = insert_tainted_value(s, ptr);
        }
      } else {
        // or when the ptr is captured
        auto new_val = insert_tainted_value(s, ptr);
      }
      continue;
    }
    if (auto *l = dyn_cast<LoadInst>(u)) {
      if (l->getType()->isPointerTy()) {
        // if a ptr is loaded we need to trace its usages
        insert_tainted_value(l, ptr);
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
        insert_tainted_value(gep, ptr);
        // ptr info will be constructed when the gep is visited
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
        insert_tainted_value(gep, ptr);
        // ptr info will be constructed when the gep is visited
      }
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
      insert_tainted_value(phi, ptr);
      // the ptr info will be properly constructed when the phi is visited
      continue;
    }
    if (auto *select = dyn_cast<SelectInst>(u)) {
      // follow the resulting ptr
      auto select_info = insert_tainted_value(select, ptr);
      if (select_info->ptr_info == nullptr) {
        select_info->ptr_info = ptr->ptr_info;
        select_info->ptr_info->add_ptr_info_user(select_info);
      } else {
        ptr->ptr_info->merge_with(select_info->ptr_info);
      }
      continue;
    }
    if (auto *ret = dyn_cast<ReturnInst>(u)) {
      visit_ptr_ret(ptr, ret);
      continue;
    }

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
  auto fun_to_precalc = insert_functions_to_include(func);

  for (auto *u : func->users()) {
    if (auto *call = dyn_cast<CallBase>(u)) {
      assert(is_tainted(call));
      assert(call->getType()->isPointerTy());

      auto call_info = insert_tainted_value(call, ptr);
      assert(call_info->ptr_info != nullptr);
      ptr->ptr_info->merge_with(call_info->ptr_info);
    }
    if (functions_that_may_be_called_indirect.find(func) !=
        functions_that_may_be_called_indirect.end()) {
      // this func may be called indirect we need to visit all indirect call
      // sites
      // TODO this could be done more efficient...
      // TODO duplicate code with taint_all_indirect_call_args
      for (auto &f : M.functions()) {
        if (is_func_from_std(&f)) {
          // avoid messing with std::'s internals
          continue;
        }
        for (auto I = inst_begin(f), E = inst_end(f); I != E; ++I) {
          if (auto *call = dyn_cast<CallBase>(&*I)) {
            auto targets = get_possible_call_targets(call);
            if (std::find(targets.begin(), targets.end(), func) !=
                targets.end()) {
              assert(is_tainted(call));
              assert(call->getType()->isPointerTy());
              auto call_info = insert_tainted_value(
                  call, TaintReason::CONTROL_FLOW_CALLEE_NEEDED);
              assert(call_info->ptr_info != nullptr);
              ptr->ptr_info->merge_with(call_info->ptr_info);
            }
          }
        }
      }
    }
  }
}

std::shared_ptr<PrecalculationFunctionAnalysis>
PrecalculationAnalysis::insert_functions_to_include(llvm::Function *func) {
  auto pos =
      std::find_if(functions_to_include.begin(), functions_to_include.end(),
                   [&func](const auto p) { return p->func == func; });

  if (pos == functions_to_include.end()) {
    errs() << "include function: " << func->getName() << "\n";
    if (is_func_from_std(func)) {
      errs() << "this function is from std::\n";

      assert(false);
      // TODO treat std functions special?
      //  sometimes the definition is also present (templated funcs from
      //  header)
    }

    if (func->getName() == "_ZSt3minImERKT_S2_S2_") {
      errs() << "NOT IS from std:: assertion?????\n";
      assert(false);
    }

    auto fun_to_precalc =
        std::make_shared<PrecalculationFunctionAnalysis>(func);
    functions_to_include.insert(fun_to_precalc);

    // used outside of a call
    bool is_func_ptr_captured = false;

    for (auto *u : func->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        errs() << "Visit\n";
        call->dump();
        auto new_val =
            insert_tainted_value(call, TaintReason::CONTROL_FLOW_CALLEE_NEEDED);
        new_val->visited =
            false; // may need to be re-visited if we discover it is important
        continue;
      } else {
        is_func_ptr_captured = true;
      }
    }
    // indirect calls
    if (is_func_ptr_captured) {
      taint_all_indirect_calls(func);
    } // otherwise no indirect calls to this possible

    return fun_to_precalc;
  } else {
    return *pos;
  }
}

void PrecalculationAnalysis::visit_arg(
    const std::shared_ptr<TaintedValue> &arg_info) {
  auto *arg = cast<Argument>(arg_info->v);
  arg_info->visited = true;

  if (arg_info->v->getType()->isPointerTy()) {
    if (arg_info->ptr_info == nullptr) {
      arg_info->ptr_info = std::make_shared<PtrUsageInfo>(arg_info);
    }
  }

  auto *func = arg->getParent();

  auto fun_to_precalc = insert_functions_to_include(func);

  if (fun_to_precalc->args_to_use.find(arg->getArgNo()) ==
      fun_to_precalc->args_to_use.end()) {

    fun_to_precalc->args_to_use.insert(arg->getArgNo());
    // else: nothing to do, this was already visited
    for (auto *u : func->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        auto *operand = call->getArgOperand(arg->getArgNo());
        auto new_val = insert_tainted_value(operand, arg_info);
        new_val->visited =
            false; // may need to re visit if we discover it is important
        if (arg_info->is_pointer()) {
          arg_info->ptr_info->merge_with(new_val->ptr_info);
        }
        continue;
      }
      if (functions_that_may_be_called_indirect.find(func) !=
          functions_that_may_be_called_indirect.end()) {
        // this func may be called indirect we need to visit all indirect call
        // sites
        taint_all_indirect_call_args(func, arg->getArgNo(), arg_info);
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
        errs() << "NEEDED IN:\n";
        v->dump();
        errs() << "Reason: " << taint_info->getReason() << " \n";
        return true;
      }
    }
  }
  return false;
}

bool should_ignore_intrinsic(Intrinsic::ID id) {

  return
      // intrinsics serving as additional annotations to the IR:
      id == Intrinsic::lifetime_start || id == Intrinsic::lifetime_end ||
      id == Intrinsic::type_test || id == Intrinsic::public_type_test ||
      id == Intrinsic::assume || id == Intrinsic::type_checked_load ||

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
      id == Intrinsic::lrint || id == Intrinsic::llrint; // NOLINT
}

void PrecalculationAnalysis::analyze_ptr_usage_in_std(
    llvm::CallBase *call, const std::shared_ptr<TaintedValue> &ptr_arg_info) {

  assert(ptr_arg_info->v->getType()->isPointerTy());
  assert(ptr_arg_info->ptr_info);
  assert(is_call_to_std(call));

  // errs() << "in : " << call->getCalledFunction()->getName() << " \n";
  // ptr_arg_info->v->dump();

  long arg_no = -1;

  for (unsigned i = 0; i < call->getNumOperands(); ++i) {
    if (call->getArgOperand(i) == ptr_arg_info->v) {
      arg_no = i;
      break;
    }
  }
  assert(arg_no != -1);

  auto *arg = call->getCalledFunction()->getArg(arg_no);
  if (not arg->hasAttribute(llvm::Attribute::ReadNone)) {
    // errs() << "Is READ\n";
    ptr_arg_info->ptr_info->setIsReadFrom(true);
  }

  if (not arg->hasAttribute(llvm::Attribute::ReadOnly)) {
    // errs() << "Is WRITTEN\n";
    ptr_arg_info->ptr_info->setIsWrittenTo(true);
  }
}

void PrecalculationAnalysis::visit_call(
    const std::shared_ptr<TaintedValue> &call_info) {
  auto *call = cast<CallBase>(call_info->v);
  assert(!call_info->visited);
  call_info->visited = true;

  std::vector<Function *> possible_targets = get_possible_call_targets(call);

  bool need_return_val = is_retval_of_call_needed(call);
  if (need_return_val) {
    if (is_allocation(call)) {
      // nothing to do, just keep this call around, it will later be replaced
      for (auto &arg : call->args()) {
        auto arg_info = insert_tainted_value(arg, call_info);
      }

    } else {
      for (auto *func : possible_targets) {
        if (func->isIntrinsic() &&
            should_ignore_intrinsic(func->getIntrinsicID())) {
          // ignore intrinsics
          continue;
        }
        if (is_func_from_std(func)) {
          // calling into std is safe, as no side effects will occur (other
          // than for the given parameters)
          //  as std is designed to have as fw side effects as possible
          // TODO implement check for exception std::rand and std::cout/cin
          // we just need to make shure all parameters are given
          for (auto &arg : call->args()) {
            auto arg_info = insert_tainted_value(arg, call_info);

            // for ptr parameters: we need to respect if the func reads/writes
            // them
            if (arg->getType()->isPointerTy()) {
              analyze_ptr_usage_in_std(call, arg_info);
            }
          }
          continue;
        }
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

  // we need to check the control flow if a exception is raised
  // if there are no resumes in called function: no need to do anything as an
  // exception cannot be raised
  if (auto *invoke = dyn_cast<InvokeInst>(call)) {
    if (is_invoke_exception_case_needed(invoke)) {

      // if it will not cause an exception, there is no need to have an invoke
      // in this case control flow will not break if we just skip this
      // function, as we know that it does not make the flow go away due to an
      // exception

      for (auto *func : possible_targets) {
        if (func->isIntrinsic() &&
            (func->getIntrinsicID() == Intrinsic::lifetime_start ||
             func->getIntrinsicID() == Intrinsic::lifetime_end ||
             func->getIntrinsicID() == Intrinsic::type_test ||
             func->getIntrinsicID() == Intrinsic::public_type_test ||
             func->getIntrinsicID() == Intrinsic::assume ||
             func->getIntrinsicID() == Intrinsic::type_checked_load)) {
          // ignore intrinsics
          continue;
        }

        if (func == mpi_func->mpi_send_init ||
            func == mpi_func->mpi_recv_init) {
          // ma need to visit it again if we find that exception control flow
          // is needed

          assert(call_info->getReason() |
                     TaintReason::CONTROL_FLOW_EXCEPTION_NEEDED &&
                 call_info->has_specific_reason());
          continue; // ignore otherwise
        }
        if (is_func_known_to_be_safe(func)) {
          continue;
        }
        if (is_func_from_std(func)) {
          // calling into std is safe, as no side effects will occur (other
          // than for the given parameters)
          //  as std is designed to haf as fw side effects as possible
          // TODO implement check for exception std::rand and std::cout/cin
          for (auto &arg : call->args()) {
            auto arg_info = insert_tainted_value(arg, call_info);

            // for ptr parameters: we need to respect if thes func reads/writes
            // them
            if (arg->getType()->isPointerTy()) {
              analyze_ptr_usage_in_std(call, arg_info);
            }
          }
          continue;
        }
        if (func->isDeclaration()) {
          call->dump();
          func->dump();
          errs() << "Reason: " << call_info->getReason() << "\n";
          errs() << "In: " << call->getFunction()->getName() << "\n";
        }
        assert(not func->isDeclaration() &&
               "cannot analyze if external function may throw an exception");
        for (auto &bb : *func) {
          if (auto *res = dyn_cast<ResumeInst>(bb.getTerminator())) {
            insert_tainted_value(res, TaintReason::CONTROL_FLOW);
          }
        }
      }
    }
  }

  if (call->isIndirectCall()) {
    // we need to taint the function ptr
    auto func_ptr_info =
        insert_tainted_value(call->getCalledOperand(), call_info);
    // may be already set if this ptr was tainted before
    if (func_ptr_info->ptr_info == nullptr) {
      func_ptr_info->ptr_info = std::make_shared<PtrUsageInfo>(func_ptr_info);
      func_ptr_info->ptr_info->setIsUsedDirectly(true);
      func_ptr_info->ptr_info->setIsCalled(true);
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

  auto *func = call->getCalledFunction();
  if (not call->isIndirectCall() &&
      (func == mpi_func->mpi_send || func == mpi_func->mpi_Isend ||
       func == mpi_func->mpi_recv || func == mpi_func->mpi_Irecv)) {
    assert(ptr_given_as_arg.size() == 1);
    if (*ptr_given_as_arg.begin() == 0) {
      ptr->v->dump();
      call->dump();
      assert(false && "Tracking Communication to get the envelope is currently "
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

  assert(not ptr_given_as_arg.empty());

  assert(ptr->ptr_info); // otherwise no need to trace this ptr usage

  auto call_info = insert_tainted_value(call, ptr);

  errs() << "Visit\n";
  call->dump();

  for (auto *func : get_possible_call_targets(call)) {

    if (func == mpi_func->mpi_comm_size || func == mpi_func->mpi_comm_rank) {
      // we know it is safe to execute these "readonly" funcs
      if (*ptr_given_as_arg.begin() == 0 && ptr_given_as_arg.size() == 1) {
        // nothing to: do only reads the communicator
        // ptr is the communicator
      } else {
        // the needed value is the result of reading the comm
        assert(*ptr_given_as_arg.begin() == 1 && ptr_given_as_arg.size() == 1);
        auto new_val = insert_tainted_value(call, ptr);
        new_val = insert_tainted_value(call->getArgOperand(0),
                                       ptr); // we also need to keep the comm
        new_val->visited = false; // may need to be revisited it we discover
                                  // that this is important
      }
      continue;
    }
    if (func == mpi_func->mpi_init || func == mpi_func->mpi_init_thread) {
      // skip: MPI_Init will only transfer the cmd line args to all processes,
      // not modify them otherwise
      continue;
    }
    if (func == mpi_func->mpi_send_init || func == mpi_func->mpi_recv_init) {
      // skip: these functions will be managed seperately anyway
      // it may be the case, that e.g. the buffer or request aliases with
      // something important
      continue;
    }
    if (is_allocation(func)) {
      // skip: alloc needs to be handled differently
      // but needs to be tainted so it will be replaced later
      continue;
    }

    if (should_exclude_function_for_debugging(func)) {
      // skip:
      // TODO
      continue;
    }

    if (func->isIntrinsic() &&
        should_ignore_intrinsic(func->getIntrinsicID())) {
      // TODO for intrinsic such as max, we need to taint args!
      // ignore intrinsics
      continue;
    }

    if (is_func_from_std(func)) {
      // calling into std is safe, as no side effects will occur (other than
      // for the given parameters)
      //  as std is designed to haf as fw side effects as possible
      // TODO implement check for exception std::rand and std::cout/cin

      analyze_ptr_usage_in_std(call, ptr);
      continue;
    }

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
        func->dump();
        assert(false);
      } else {
        auto new_val = insert_tainted_value(arg, ptr);
        if (new_val->ptr_info == nullptr) {
          new_val->ptr_info = ptr->ptr_info;
          new_val->ptr_info->add_ptr_info_user(new_val);
        } else {
          ptr->ptr_info->merge_with(new_val->ptr_info);
        }
        visit_ptr_usages(new_val);
      }
    }
  }
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
    // insert what is necessary for Control flow to go here
    insert_necessary_control_flow(v);
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
    llvm::Value *v, const std::shared_ptr<TaintedValue> &from) {
  std::shared_ptr<TaintedValue> inserted_elem = nullptr;

  if (not is_tainted(v)) {

    if (auto *inst = dyn_cast<Instruction>(v)) {
      // dont analyze std::s internals
      if (is_func_from_std(inst->getFunction())) {
        // inst->getFunction()->dump();

        inst->dump();
        errs() << "In: " << inst->getFunction()->getName() << "\n";
      }
      assert(not is_func_from_std(inst->getFunction()));
    }

    // only if not already in set
    inserted_elem = std::make_shared<TaintedValue>(v);
    if (v->getType()->isPointerTy()) {
      // create empty info
      inserted_elem->ptr_info = std::make_shared<PtrUsageInfo>(inserted_elem);
    }
    if (from != nullptr) {
      // we dont care why the Control flow was tagged for te parent
      inserted_elem->addReason(from->getReason() &
                               TaintReason::REASONS_TO_PROPERGATE);
      inserted_elem->parents.insert(from);
      from->children.insert(inserted_elem);
    }
    tainted_values.insert(inserted_elem);
    // insert what is necessary for Control flow to go here
    insert_necessary_control_flow(v);
  } else {
    // the present value form the set
    inserted_elem = *std::find_if(tainted_values.begin(), tainted_values.end(),
                                  [&v](const auto &vv) { return vv->v == v; });
    if (from != nullptr) {
      inserted_elem->parents.insert(from);
      from->children.insert(inserted_elem);
      // we dont care why the Control flow was tagged for te parent
      inserted_elem->addReason(from->getReason() &
                               TaintReason::REASONS_TO_PROPERGATE);
    }
  }

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
        auto new_val = insert_tainted_value(term, TaintReason::CONTROL_FLOW);
        if (auto *invoke = dyn_cast<InvokeInst>(term)) {
          if (invoke->getUnwindDest() == bb) {
            new_val->addReason(TaintReason::CONTROL_FLOW_EXCEPTION_NEEDED);
            // it may need to be re-visited if we find out that we do need the
            // exception path
            new_val->visited = false;
          } else {
            assert(invoke->getNormalDest() == bb);
          }
        }
      }
    } else {
      // BB is function entry block
      insert_functions_to_include(bb->getParent());
    }
  }
}

void PrecalculationAnalysis::find_functions_called_indirect() {
  for (auto &f : M.functions()) {
    for (auto *u : f.users()) {
      if (not isa<CallBase>(u)) {
        functions_that_may_be_called_indirect.insert(&f);
      }
    }
  }
}

std::vector<llvm::Function *>
PrecalculationAnalysis::get_possible_call_targets(llvm::CallBase *call) {
  std::vector<llvm::Function *> possible_targets;
  if (call->isIndirectCall()) {
    possible_targets = virtual_call_sites.get_possible_call_targets(call);
  } else {
    possible_targets.push_back(call->getCalledFunction());
  }
  if (possible_targets.empty()) {
    // can call any function with same type that we get a ptr of somewhere
    for (auto *func : functions_that_may_be_called_indirect) {
      if (func->getFunctionType() == call->getFunctionType())
        possible_targets.push_back(func);
    }
    // TODO can we check that we will not be able to get a ptr to a function
    // outside of the module?
  }

  if (possible_targets.empty()) {
    call->dump();
    errs() << "In: " << call->getFunction()->getName() << "\n";
  }

  assert(not possible_targets.empty() && "could not find tgts of call");
  return possible_targets;
}

void PrecalculationAnalysis::taint_all_indirect_call_args(
    llvm::Function *func, unsigned int ArgNo,
    const std::shared_ptr<TaintedValue> &arg_info) {
  // TODO this could be done more efficient...
  for (auto &f : M.functions()) {
    if (is_func_from_std(&f)) {
      // avoid messing with std::'s internals
      // if std:: indirectely calls a user function, it needs to be given as
      // an argument anyway
      continue;
    }
    for (auto I = inst_begin(f), E = inst_end(f); I != E; ++I) {
      if (auto *call = dyn_cast<CallBase>(&*I)) {
        auto targets = get_possible_call_targets(call);
        if (std::find(targets.begin(), targets.end(), func) != targets.end()) {
          auto new_info =
              insert_tainted_value(call->getArgOperand(ArgNo), arg_info);
          new_info->visited =
              false; // may need to re-visit if we discover it is important
          if (arg_info->is_pointer()) {
            assert(arg_info->ptr_info);
            if (new_info->ptr_info == nullptr) {
              new_info->ptr_info = arg_info->ptr_info;
              new_info->ptr_info->add_ptr_info_user(new_info);
            } else {
              arg_info->ptr_info->merge_with(new_info->ptr_info);
            }
          }
        }
      }
    }
  }
}

void PrecalculationAnalysis::taint_all_indirect_calls(llvm::Function *func) {
  errs() << "INDIRECT CALLS TO: " << func->getName() << "\n";

  // TODO this could be done more efficient...
  // TODO duplicate code with taint_all_indirect_call_args
  for (auto &f : M.functions()) {
    if (is_func_from_std(&f)) {
      // avoid messing with std::'s internals
      // if std:: indireclty calls a user function, it needs to be given as an
      // argument anyway
      continue;
    }
    for (auto I = inst_begin(f), E = inst_end(f); I != E; ++I) {
      if (auto *call = dyn_cast<CallBase>(&*I)) {
        auto targets = get_possible_call_targets(call);
        if (std::find(targets.begin(), targets.end(), func) != targets.end()) {
          insert_tainted_value(call, TaintReason::CONTROL_FLOW_CALLEE_NEEDED);
        }
      }
    }
  }
}
void PrecalculationAnalysis::print_analysis_result_remarks() {

  for (const auto &v : tainted_values) {
    if (auto *inst = dyn_cast<Instruction>(v->v)) {
      errs() << "need for reason: " << v->getReason() << "\n";
      errs() << inst->getFunction()->getName() << "\n";
      inst->dump();
    }
  }
  debug_printings();
}

void PrecalculationAnalysis::debug_printings() {
  errs() << "ADDITIONAL DEBUG PRINTING\n";

  for (const auto &v : tainted_values) {
    if (v->v->getName() == "this") {
      v->ptr_info->dump();
      break;
    }
  }
}
const std::set<std::shared_ptr<PrecalculationFunctionAnalysis>> &
PrecalculationAnalysis::getFunctionsToInclude() const {
  return functions_to_include;
}
Function *PrecalculationAnalysis::getEntryPoint() const { return entry_point; }
const std::vector<llvm::CallBase *> &

PrecalculationAnalysis::getToReplaceWithEnvelopeRegister() const {
  return to_replace_with_envelope_register;
}
