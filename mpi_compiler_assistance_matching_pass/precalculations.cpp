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

#include "analysis_results.h"
#include "conflict_detection.h"
#include "devirt_analysis.h"
#include "mpi_functions.h"
#include "precalculation.h"

#include "implementation_specific.h"
#include "mpi_functions.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/IR/Verifier.h"

#include "debug.h"
using namespace llvm;

void print_childrens(std::shared_ptr<TaintedValue> parent,
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

void print_parents(std::shared_ptr<TaintedValue> child,
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

// True if callee needs to be called as it contains tainted instructions
// or True if callee can raise an exception and the exception handling code is
// actually tainted if exception handling is not tainted, we dont need to handle
// the exception anyway and abortion is fine in this case
bool Precalculations::is_invoke_necessary_for_control_flow(
    llvm::InvokeInst *invoke) {

  assert(invoke);

  if (invoke->isIndirectCall()) {
    // TODO if we know by devirt analysis that call is not neded we can exclude
    // it
    return true;
  }
  // calling into something we need
  if (std::find_if(functions_to_include.begin(), functions_to_include.end(),
                   [&invoke](auto f) {
                     return f->F_orig == invoke->getCalledFunction();
                   }) != functions_to_include.end()) {
    return true;
  }

  // check if the exception part is needed
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

bool should_exclude_function_for_debugging(llvm::Function *func) {
  if (is_mpi_function(func)) {
    return true;
  }
  return false;
}

bool is_allocation(Function *func) {
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

bool is_allocation(llvm::CallBase *call) {
  // operator new
  if (call->getCalledFunction()->getName() == "_Znwm") {
    assert(isa<Constant>(call->getArgOperand(0)) &&
           "Non constant allocation in new??");
    return true;
  }
  return is_allocation(call->getCalledFunction());
}

bool is_free(Function *func) {
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
  return is_free(call->getCalledFunction());
}

void Precalculations::add_precalculations(
    const std::vector<llvm::CallBase *> &to_precompute) {
  to_replace_with_envelope_register = to_precompute;

  for (auto call : to_precompute) {
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

  auto vtm = VtableManager(M);
  for (const auto &f : functions_to_include) {
    f->initialize_copy();
    vtm.register_function_copy(f->F_orig, f->F_copy);
  }

  vtm.perform_vtable_change_in_copies();
  // dont fuse this loops! we first need to initialize the copy before changing
  // calls
  for (const auto &f : functions_to_include) {
    replace_calls_in_copy(f);
  }

  for (const auto &f : functions_to_include) {
    prune_function_copy(f);
  }
  add_call_to_precalculation_to_main();
}

void Precalculations::find_all_tainted_vals() {
  unsigned long num_visited = 0;

  std::vector<std::shared_ptr<TaintedValue>> to_visit;

  do {
    to_visit.clear();
    std::copy_if(tainted_values.begin(), tainted_values.end(),
                 std::back_inserter(to_visit),
                 [](const auto &v) { return !v->visited; });

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
  for (auto bb : tainted_blocks) {
    for (auto pred = pred_begin(bb); pred != pred_end(bb); ++pred) {
      assert(tainted_blocks.find(*pred) != tainted_blocks.end());
    }
  }
}

void Precalculations::visit_load(
    const std::shared_ptr<TaintedValue> &load_info) {
  assert(not load_info->visited);
  load_info->visited = true;
  auto load = dyn_cast<LoadInst>(load_info->v);
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

void Precalculations::visit_store_from_value(
    const std::shared_ptr<TaintedValue> &store_info) {
  auto store = dyn_cast<StoreInst>(store_info->v);
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

void Precalculations::visit_store_from_ptr(
    const std::shared_ptr<TaintedValue> &store_info) {
  auto store = dyn_cast<StoreInst>(store_info->v);
  assert(store);

  assert(is_tainted(store->getPointerOperand()));
  // TODO do we need to do something here?
  auto new_val = insert_tainted_value(store->getValueOperand(), store_info);
  // we should assert that this ptr usage is already covered by the ptr usage
  // info
  auto ptr = insert_tainted_value(store->getPointerOperand(), store_info);
  assert(
      ptr->ptr_info
          ->isUsedDirectly()); // must already be set, otherwise there is no
                               // need in visiting this store in the first place
  ptr->ptr_info->setIsWrittenTo(true);
}

void Precalculations::visit_store(
    const std::shared_ptr<TaintedValue> &store_info) {
  assert(not store_info->visited);
  store_info->visited = true;
  auto store = dyn_cast<StoreInst>(store_info->v);
  assert(store);

  if (is_tainted(store->getValueOperand())) {
    visit_store_from_value(store_info);
  }
  if (is_tainted(store->getPointerOperand())) {
    visit_store_from_ptr(store_info);
  }
}

void Precalculations::visit_gep(const std::shared_ptr<TaintedValue> &gep_info) {
  assert(not gep_info->visited);
  gep_info->visited = true;
  auto gep = dyn_cast<GetElementPtrInst>(gep_info->v);
  assert(gep);
  bool coming_from_ptr = (is_tainted(gep->getPointerOperand()));

  auto gep_ptr_info = insert_tainted_value(gep->getPointerOperand(), gep_info);

  if (gep_ptr_info->ptr_info == nullptr) {
    gep_ptr_info->ptr_info = std::make_shared<PtrUsageInfo>(gep_ptr_info);
  }
  if (gep_info->ptr_info == nullptr) {
    gep_info->ptr_info = std::make_shared<PtrUsageInfo>(gep_info);
  }

  if (gep->hasAllConstantIndices()) {
    if (isa<GetElementPtrInst>(gep_ptr_info->v)) {
      errs() << "Use a pass that combines GEP instructions first\n";
      assert(false);
    }
    std::vector<unsigned int> idxs;
    for (auto &idx : gep->indices()) {
      auto idx_constant = dyn_cast<ConstantInt>(&idx);
      assert(idx_constant);
      unsigned int idx_v = idx_constant->getZExtValue();
      idxs.push_back(idx_v);
      if (not coming_from_ptr) {
        gep_ptr_info->ptr_info->add_important_member(idxs, gep_info->ptr_info);
      } else {
        // coming from ptr
        // check if value is needed and de-taint if not
        if (not gep_ptr_info->ptr_info->isWholePtrIsRelevant()) {
          if (not gep_ptr_info->ptr_info->is_member_relevant(idxs)) {
            // de-taint non relevant access
            remove_tainted_value(gep_info);
          }
        }
      }
    }
  } else {
    // we could not determine which fields are important, need to keep track of
    // everything
    // as this ptr access may override the other important fields
    if (gep_ptr_info->ptr_info != gep_info->ptr_info) {
      gep_ptr_info->ptr_info->merge_with(gep_info->ptr_info);
      gep_ptr_info->ptr_info->setWholePtrIsRelevant(true);
    }
    assert(gep_ptr_info->ptr_info == gep_info->ptr_info);
    assert(gep_ptr_info->ptr_info->isWholePtrIsRelevant());
  }
}

void Precalculations::visit_phi(const std::shared_ptr<TaintedValue> &phi_info) {
  auto phi = dyn_cast<PHINode>(phi_info->v);
  assert(phi);

  if (phi->getType()->isPointerTy()) {
    auto ptr_info = phi_info->ptr_info;
    bool ptr_info_was_present = (ptr_info != nullptr);
    if (not ptr_info) {
      ptr_info = std::make_shared<PtrUsageInfo>(phi_info);
    }
    for (unsigned int i = 0; i < phi->getNumOperands(); ++i) {
      auto vv = phi->getIncomingValue(i);
      auto new_val = insert_tainted_value(vv, phi_info);

      if (new_val->ptr_info) {
        ptr_info_was_present = true;
        ptr_info->merge_with(new_val->ptr_info);
      } else {
        new_val->ptr_info = ptr_info;
        ptr_info->add_ptr_info_user(new_val);
      }
    }
    assert(ptr_info_was_present);
  } else {
    // no ptr
    for (unsigned int i = 0; i < phi->getNumOperands(); ++i) {
      auto vv = phi->getIncomingValue(i);
      auto new_val = insert_tainted_value(vv, phi_info);
    }
  }

  phi_info->visited = true;
}

void Precalculations::visit_val(std::shared_ptr<TaintedValue> v) {
  errs() << "Visit\n";
  v->v->dump();

  if (v->is_pointer()) {
    visit_ptr_usages(v);
  }

  if (auto *c = dyn_cast<Constant>(v->v)) {
    // nothing to do for constant
    v->visited = true;
    return;
  }
  if (auto *l = dyn_cast<LoadInst>(v->v)) {
    visit_load(v);
    return;
  }
  if (auto *a = dyn_cast<AllocaInst>(v->v)) {
    // nothing to do: visit_ptr_usages is called on all ptrs anyway
    v->visited = true;
    return;
  }
  if (auto *s = dyn_cast<StoreInst>(v->v)) {
    visit_store(v);
    return;
  }
  if (auto *op = dyn_cast<BinaryOperator>(v->v)) {
    // arithmetic
    // TODO do we need to exclude some opcodes?
    assert(op->getNumOperands() == 2);
    assert(not op->getType()->isPointerTy());
    insert_tainted_value(op->getOperand(0), v);
    insert_tainted_value(op->getOperand(1), v);
    v->visited = true;
    return;
  }
  if (auto *op = dyn_cast<CmpInst>(v->v)) {
    // cmp
    assert(op->getNumOperands() == 2);
    // TODO handle pointer case?
    assert(not op->getOperand(0)->getType()->isPointerTy());
    assert(not op->getOperand(1)->getType()->isPointerTy());
    insert_tainted_value(op->getOperand(0), v);
    insert_tainted_value(op->getOperand(1), v);
    v->visited = true;
    return;
  }
  if (auto *select = dyn_cast<SelectInst>(v->v)) {
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
    return;
  }
  if (auto *arg = dyn_cast<Argument>(v->v)) {
    visit_arg(v);
    return;
  }
  if (auto *call = dyn_cast<CallBase>(v->v)) {
    visit_call(v);
    return;
  }
  if (auto *phi = dyn_cast<PHINode>(v->v)) {
    visit_phi(v);
    return;
  }
  if (auto *cast = dyn_cast<CastInst>(v->v)) {
    // ptr casting is not supported
    assert(not cast->getType()->isPointerTy());
    assert(not cast->getOperand(0)->getType()->isPointerTy());
    insert_tainted_value(cast->getOperand(0), v);
    v->visited = true;
    return;
  }
  if (auto *gep = dyn_cast<GetElementPtrInst>(v->v)) {
    visit_gep(v);
    assert(is_tainted(gep->getPointerOperand()));
    v->visited = true;
    return;
  }
  if (auto *br = dyn_cast<BranchInst>(v->v)) {
    assert(v->reason == TaintReason::CONTROL_FLOW);
    v->visited = true;
    if (br->isConditional()) {
      insert_tainted_value(br->getCondition(), v);
    } else {
      // nothing to do
    }
    return;
  }

  errs() << "Support for analyzing this Value is not implemented yet\n";
  v->v->dump();
  assert(false);
}

void Precalculations::visit_ptr_usages(std::shared_ptr<TaintedValue> ptr) {
  assert(ptr->is_pointer());

  if (auto global = dyn_cast<GlobalVariable>(ptr->v)) {
    auto implementation_specifics = ImplementationSpecifics::get_instance();
    if (global == implementation_specifics->COMM_WORLD) {
      // no need to handle all usages of Comm World as we know it is a static
      // object
      return;
    }
  }

  if (ptr->ptr_info == nullptr) {
    // this pointer is not needed
    // if this pointer is indeed needed it will later be visited again if the
    // ptr info was initialized
    return;
  }

  for (auto u : ptr->v->users()) {
    if (auto *s = dyn_cast<StoreInst>(u)) {
      // if we dont read the ptr directly, we dont need to capture the stores
      //  e.g. a struct ptr where the first member is not used
      if (s->getPointerOperand() == ptr->v) {
        // all stores to this ptr
        if (ptr->ptr_info->isUsedDirectly() ||
            ptr->ptr_info->isWholePtrIsRelevant()) {
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
      // the test if the resulting GEP is actually relevant with the ptr info
      // is conducted when the gep is visited
      // if not relevant: it will be removed from tainted values
      insert_tainted_value(gep, ptr);
      continue;
    }
    if (auto *compare = dyn_cast<ICmpInst>(u)) {
      // nothing to do
      // the compare will be tainted if it is necessary for control flow else
      // we can ignore it
      continue;
    }
    if (auto *phi = dyn_cast<PHINode>(u)) {
      // follow the phi
      insert_tainted_value(phi, ptr);
      // the ptr info will be constructed when the phi is visited
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
    errs() << "Support for analyzing this Value is not implemented yet\n";
    u->dump();
    assert(false);
  }
}

std::shared_ptr<FunctionToPrecalculate>
Precalculations::insert_functions_to_include(llvm::Function *func) {
  auto pos =
      std::find_if(functions_to_include.begin(), functions_to_include.end(),
                   [&func](const auto p) { return p->F_orig == func; });

  if (pos == functions_to_include.end()) {
    errs() << "include function: " << func->getName() << "\n";
    auto fun_to_precalc = std::make_shared<FunctionToPrecalculate>(func);
    functions_to_include.insert(fun_to_precalc);

    for (auto u : func->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        errs() << "Visit\n";
        call->dump();
        auto new_val = insert_tainted_value(call, TaintReason::CONTROL_FLOW);
        continue;
      }
    }
    // indirect calls
    taint_all_indirect_calls(func);

    return fun_to_precalc;
  } else {
    return *pos;
  }
}

void Precalculations::visit_arg(std::shared_ptr<TaintedValue> arg_info) {
  auto *arg = cast<Argument>(arg_info->v);
  arg_info->visited = true;

  auto *func = arg->getParent();

  auto fun_to_precalc = insert_functions_to_include(func);

  if (fun_to_precalc->args_to_use.find(arg->getArgNo()) ==
      fun_to_precalc->args_to_use.end()) {

    fun_to_precalc->args_to_use.insert(arg->getArgNo());
    // else: nothing to do, this was already visited
    for (auto u : func->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        auto *operand = call->getArgOperand(arg->getArgNo());
        auto new_val = insert_tainted_value(operand, arg_info);
        if (arg_info->is_pointer()) {
          if (new_val->ptr_info == nullptr) {
            new_val->ptr_info = arg_info->ptr_info;
            new_val->ptr_info->add_ptr_info_user(new_val);
          } else {
            arg_info->ptr_info->merge_with(new_val->ptr_info);
          }
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

bool Precalculations::is_retval_of_call_used(llvm::CallBase *call) {
  // check if this is tainted as the ret val is used

  std::set<Value *> users_of_retval;
  std::transform(call->use_begin(), call->use_end(),
                 std::inserter(users_of_retval, users_of_retval.begin()),
                 [](const auto &u) { return dyn_cast<Value>(&u); });
  bool need_return_val = not is_none_tainted(users_of_retval);
  return need_return_val;
}

void Precalculations::visit_call(std::shared_ptr<TaintedValue> call_info) {
  auto *call = cast<CallBase>(call_info->v);
  assert(!call_info->visited);
  call_info->visited = true;

  std::vector<Function *> possible_targets = get_possible_call_targets(call);

  bool need_return_val = is_retval_of_call_used(call);
  if (need_return_val) {
    if (is_allocation(call)) {
      // nothing to do, just keep this call around, it will later be replaced

    } else {
      for (auto func : possible_targets) {
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
        if (func->isDeclaration()) {
          errs() << "\n";
          call->dump();
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
    if (is_invoke_necessary_for_control_flow(invoke)) {
      // if it will not cause an exception, there is no need to have an invoke
      // in this case control flow will not break if we just skip this
      // function, as we know that it does not make the flow go away due to an
      // exception

      for (auto func : possible_targets) {
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
        assert(not func->isDeclaration() &&
               "cannot analyze if external function may throw an exception");
        for (auto &bb : *func) {
          if (auto *res = dyn_cast<ResumeInst>(bb.getTerminator())) {
            insert_tainted_value(res, call_info);
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

void Precalculations::visit_call_from_ptr(llvm::CallBase *call,
                                          std::shared_ptr<TaintedValue> ptr) {

  if (call->getCalledOperand() == ptr->v) {
    // visit from the function ptr: nothing to check
    return;
  }

  assert(ptr->ptr_info); // otherwise no need to trace this ptr usage

  auto call_info = insert_tainted_value(call, ptr);

  errs() << "Visit\n";
  call->dump();

  std::set<unsigned int> ptr_given_as_arg;
  for (unsigned int i = 0; i < call->arg_size(); ++i) {
    if (call->getArgOperand(i) == ptr->v) {
      ptr_given_as_arg.insert(i);
    }
  }
  assert(not ptr_given_as_arg.empty());

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
        insert_tainted_value(call->getArgOperand(0),
                             ptr); // we also need to keep the comm
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
    if (is_allocation(func) || is_free(func)) {
      // skip: alloc/free need to be handled differently
      continue;
    }

    if (should_exclude_function_for_debugging(func)) {
      // skip:
      // TODO
      continue;
    }

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
Precalculations::insert_tainted_value(llvm::Value *v, TaintReason reason) {

  auto dummy = std::make_shared<TaintedValue>(nullptr);
  dummy->reason = reason;

  return insert_tainted_value(v, dummy);
}

void propergate_state_to_children(const std::shared_ptr<TaintedValue> &parent) {
  assert(parent != nullptr);
  for (auto child : parent->children) {
    if (child != nullptr && child->reason != parent->reason) {
      child->reason = child->reason | parent->reason;
      propergate_state_to_children(child);
    }
  }
}

std::shared_ptr<TaintedValue>
Precalculations::insert_tainted_value(llvm::Value *v,
                                      std::shared_ptr<TaintedValue> from) {
  std::shared_ptr<TaintedValue> inserted_elem = nullptr;

  if (not is_tainted(v)) {
    // only if not already in set
    inserted_elem = std::make_shared<TaintedValue>(v);
    if (from != nullptr) {
      inserted_elem->reason = from->reason;
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
      propergate_state_to_children(from); // propergate the taint Reason
    }
  }

  assert(inserted_elem != nullptr);
  return inserted_elem;
}

void Precalculations::insert_necessary_control_flow(Value *v) {
  if (auto *inst = dyn_cast<Instruction>(v)) {
    auto bb = inst->getParent();
    if (not bb->isEntryBlock()) {
      // we need to insert the instruction that lets the control flow go
      // here
      for (auto pred_bb : predecessors(bb)) {
        auto *term = pred_bb->getTerminator();
        insert_tainted_value(term, TaintReason::CONTROL_FLOW);
      }
    } else {
      // BB is function entry block
      insert_functions_to_include(bb->getParent());
    }
  }
}

void FunctionToPrecalculate::initialize_copy() {
  assert(F_copy == nullptr);
  assert(not F_orig->isDeclaration() && "Cannot copy external function");
  F_copy = CloneFunction(F_orig, old_new_map, cloned_code_info);

  for (auto v : old_new_map) {
    Value *old_v = const_cast<Value *>(v.first);
    Value *new_v = v.second;
    new_to_old_map.insert(std::make_pair(new_v, old_v));
  }
}

void Precalculations::replace_calls_in_copy(
    std::shared_ptr<FunctionToPrecalculate> func) {
  std::vector<CallBase *> to_replace;

  // first  gather calls that need replacement so that the iterator does not
  // get broken if we remove stuff

  for (auto I = inst_begin(func->F_copy), E = inst_end(func->F_copy); I != E;
       ++I) {
    if (auto *call = dyn_cast<CallBase>(&*I)) {
      auto callee = call->getCalledFunction();
      if (callee == mpi_func->mpi_comm_rank ||
          callee == mpi_func->mpi_comm_size) {
        continue; // noting to do, keep original call
      }
      if (callee == mpi_func->mpi_send_init) {
        to_replace.push_back(call);
        continue;
      }
      if (callee == mpi_func->mpi_recv_init) {
        to_replace.push_back(call);
        continue;
      }
      // end handling calls to MPI

      // TODO code duplication wir auto pos=
      auto pos =
          std::find_if(functions_to_include.begin(), functions_to_include.end(),
                       [&call](const auto p) {
                         return p->F_orig == call->getCalledFunction();
                       });
      if (pos != functions_to_include.end()) {

        to_replace.push_back(call);
        continue;
      } else {
        // indirect call
        if (call->isIndirectCall()) {
          to_replace.push_back(call);
        } else {
          assert(not is_tainted(callee));
          // it is not used: nothing to do
        }
      }
    }
  }

  for (auto *call : to_replace) {
    auto callee = call->getCalledFunction();
    if (callee == mpi_func->mpi_send_init) {
      auto tag = get_tag_value(call, true);
      auto src = get_src_value(call, true);
      IRBuilder<> builder = IRBuilder<>(call);

      CallBase *new_call = nullptr;
      if (auto *invoke = dyn_cast<InvokeInst>(call)) {
        new_call = builder.CreateInvoke(mpi_func->optimized.register_send_tag,
                                        invoke->getNormalDest(),
                                        invoke->getUnwindDest(), {src, tag});
      } else {
        new_call = builder.CreateCall(mpi_func->optimized.register_send_tag,
                                      {src, tag});
      }
      call->replaceAllUsesWith(new_call);
      call->eraseFromParent();
      auto old_call_v = func->new_to_old_map[call];
      func->new_to_old_map[new_call] = old_call_v;

      continue;
    }
    if (callee == mpi_func->mpi_recv_init) {
      auto tag = get_tag_value(call, true);
      auto src = get_src_value(call, true);
      IRBuilder<> builder = IRBuilder<>(call);
      CallBase *new_call = nullptr;
      if (auto *invoke = dyn_cast<InvokeInst>(call)) {
        new_call = builder.CreateInvoke(mpi_func->optimized.register_recv_tag,
                                        invoke->getNormalDest(),
                                        invoke->getUnwindDest(), {src, tag});
      } else {
        new_call = builder.CreateCall(mpi_func->optimized.register_recv_tag,
                                      {src, tag});
      }
      call->replaceAllUsesWith(new_call);
      call->eraseFromParent();
      auto old_call_v = func->new_to_old_map[call];
      func->new_to_old_map[new_call] = old_call_v;

      continue;
    }
    // end handling calls to MPI

    auto pos = std::find_if(functions_to_include.begin(),
                            functions_to_include.end(), [&call](const auto p) {
                              if (call->isIndirectCall()) {
                                // indirect call: any function with same
                                // signature
                                return p->F_orig->getFunctionType() ==
                                       call->getFunctionType();
                              } else {
                                // direct call
                                return p->F_orig == call->getCalledFunction();
                              }
                            });
    if (pos == functions_to_include.end() && not call->isIndirectCall()) {
      // special case: it is a invoke inst and we later find out that it
      // actually has no resume -> meaning no exception and retval is not used
      auto *invoke = dyn_cast<InvokeInst>(call);
      assert(invoke);
      IRBuilder<> builder = IRBuilder<>(invoke);
      builder.CreateBr(invoke->getNormalDest());
      invoke->eraseFromParent();
      // if there were an exception, or the result value is used, the callee
      // would have been tainted
      continue;
    }

    const auto &function_information = *pos;

    if (not call->isIndirectCall()) {
      call->setCalledFunction(function_information->F_copy);
    }
    // null all not used args

    Value *orig_call_v = func->new_to_old_map[call];
    auto *orig_call = dyn_cast<CallBase>(orig_call_v);
    assert(orig_call);

    for (unsigned int i = 0; i < call->arg_size(); ++i) {
      if (not is_tainted(orig_call->getArgOperand(i))) {
        // set unused arg to 0 (so we dont need to compute it)
        call->setArgOperand(
            i, Constant::getNullValue(call->getArgOperand(i)->getType()));
      } // else pass arg normally
    }
  }

  replace_usages_of_func_in_copy(func);
}

// remove all unnecessary instruction
void Precalculations::prune_function_copy(
    const std::shared_ptr<FunctionToPrecalculate> &func) {
  std::vector<Instruction *> to_prune;

  // first  gather all instructions, so that the iterator does not get broken
  // if we remove stuff
  for (auto I = inst_begin(func->F_copy), E = inst_end(func->F_copy); I != E;
       ++I) {

    Instruction *inst = &*I;
    auto old_v = func->new_to_old_map[inst];
    if (not is_tainted(old_v)) {
      to_prune.push_back(inst);
    } else if (auto *invoke = dyn_cast<InvokeInst>(inst)) {
      // an invoke can be tainted only because it may return an exception
      // but it actually is exception free for our purpose
      // meaning if it throws no MPI is used
      if (not is_invoke_necessary_for_control_flow(
              dyn_cast<InvokeInst>(old_v)) &&
          not is_retval_of_call_used(dyn_cast<InvokeInst>(old_v))) {
        to_prune.push_back(inst);
      }
    }
  }

  // remove stuff
  for (auto inst : to_prune) {
    if (inst->isTerminator()) {
      if (auto *invoke = dyn_cast<InvokeInst>(inst)) {
        // an invoke that was determined not necessary will just be skipped
        IRBuilder<> builder = IRBuilder<>(inst);
        builder.CreateBr(invoke->getNormalDest());
      } else {
        // if this terminator was not tainted: we can immediately return from
        // this function
        IRBuilder<> builder = IRBuilder<>(inst);
        if (inst->getFunction()->getReturnType()->isVoidTy()) {
          builder.CreateRetVoid();
        } else {
          builder.CreateRet(
              Constant::getNullValue(inst->getFunction()->getReturnType()));
        }
      }
    }

    // we keep the exception handling instructions so that the module is still
    // correct if they are not tainted and an exception occurs we abort anyway
    // (otherwise we would have tainted the exception handling code)
    if (auto lp = dyn_cast<LandingPadInst>(inst)) {
      // lp->setCleanup(false);
      lp->dump();
    } else {
      inst->replaceAllUsesWith(UndefValue::get(inst->getType()));
      inst->eraseFromParent();
    }
  }

  // perform DCE by removing now unused BBs
  std::set<BasicBlock *> to_remove_bb;
  for (auto &BB : *func->F_copy) {
    if (pred_empty(&BB) && not BB.isEntryBlock()) {
      to_remove_bb.insert(&BB);
    }
  }
  // and remove BBs
  for (auto *BB : to_remove_bb) {
    BB->eraseFromParent();
  }

  // one can now also combine blocks
}

void Precalculations::add_call_to_precalculation_to_main() {
  // TODO code duplication wir auto pos=
  auto pos = std::find_if(
      functions_to_include.begin(), functions_to_include.end(),
      [this](const auto p) { return p->F_orig == this->entry_point; });
  assert(pos != functions_to_include.end());
  const auto &function_info = *pos;
  auto entry_to_precalc = function_info->F_copy;

  // search for MPI_init or Init Thread as precalc may only take place after
  // that
  CallBase *call_to_init = nullptr;
  if (mpi_func->mpi_init != nullptr) {
    for (auto u : mpi_func->mpi_init->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        assert(call_to_init == nullptr && "MPI_Init is only allowed once");
        call_to_init = call;
      }
    }
  }
  if (mpi_func->mpi_init_thread != nullptr) {
    for (auto u : mpi_func->mpi_init_thread->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        assert(call_to_init == nullptr && "MPI_Init is only allowed once");
        call_to_init = call;
      }
    }
  }

  assert(call_to_init != nullptr && "Did Not Found MPI_Init_Call");

  assert(call_to_init->getFunction() == entry_point &&
         "MPI_Init is not in main");

  // insert after init
  // MPIOPT_Init will later be inserted between this 2 calls
  IRBuilder<> builder(call_to_init->getNextNode());

  // forward args of main
  std::vector<Value *> args;
  for (auto &arg : entry_point->args()) {
    args.push_back(&arg);
  }
  builder.CreateCall(function_info->F_copy, args);
  auto re_init_fun = get_global_re_init_function();
  builder.CreateCall(re_init_fun);
  builder.CreateCall(mpi_func->optimized.check_registered_conflicts);
}

void Precalculations::find_functions_called_indirect() {
  for (auto &f : M.functions()) {
    for (auto u : f.users()) {
      if (not isa<CallBase>(u)) {
        functions_that_may_be_called_indirect.insert(&f);
      }
    }
  }
}

void Precalculations::replace_usages_of_func_in_copy(
    std::shared_ptr<FunctionToPrecalculate> func) {
  std::vector<Instruction *> instructions_to_change;
  for (auto u : func->F_orig->users()) {
    if (auto *inst = dyn_cast<Instruction>(u)) {

      auto pos = std::find_if(
          functions_to_include.begin(), functions_to_include.end(),
          [&inst](const auto p) { return p->F_copy == inst->getFunction(); });
      if (pos != functions_to_include.end()) {
        if (not isa<CallBase>(inst)) {
          // calls are replaced by a different function
          // where we also take care about the arguments
          instructions_to_change.push_back(inst);
        }
      } // else: a use in the original version of a function
      continue;
    }
    if (isa<ConstantAggregate>(u)) {
      // an array of function ptrs -- aka a vtable for objects
      // nothing to do, the vtable manager will take care of this
      continue;
    }
    errs() << "This usage is currently not supported:\n";
    errs() << func->F_orig->getName() << "\n";
    u->dump();
    assert(false);
  }

  for (auto *inst : instructions_to_change) {
    bool has_replaced = inst->replaceUsesOfWith(func->F_orig, func->F_copy);
    assert(has_replaced);
  }
}
std::vector<llvm::Function *>
Precalculations::get_possible_call_targets(llvm::CallBase *call) {
  std::vector<llvm::Function *> possible_targets;
  if (call->isIndirectCall()) {
    possible_targets = virtual_call_sites.get_possible_call_targets(call);
  } else {
    possible_targets.push_back(call->getCalledFunction());
  }
  if (possible_targets.empty()) {
    // can call any function with same type that we get a ptr of somewhere
    for (auto func : functions_that_may_be_called_indirect) {
      if (func->getFunctionType() == call->getFunctionType())
        possible_targets.push_back(func);
    }
    // TODO can we check that we will not be able to get a ptr to a function
    // outside of the module?
  }
  assert(not possible_targets.empty() && "could not find tgts of call");
  return possible_targets;
}

void Precalculations::taint_all_indirect_call_args(llvm::Function *func, unsigned int ArgNo,
    std::shared_ptr<TaintedValue> arg_info) {
  // TODO this could be done more efficient...
  for (auto &f : M.functions()) {
    for (auto I = inst_begin(f), E = inst_end(f); I != E; ++I) {
      if (auto *call = dyn_cast<CallBase>(&*I)) {
        auto targets = get_possible_call_targets(call);
        if (std::find(targets.begin(), targets.end(), func) != targets.end()) {
          auto new_info =
              insert_tainted_value(call->getArgOperand(ArgNo), arg_info);
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

void Precalculations::taint_all_indirect_calls(llvm::Function *func) {
  // TODO this could be done more efficient...
  // TODO duplicate code with taint_all_indirect_call_args
  for (auto &f : M.functions()) {
    for (auto I = inst_begin(f), E = inst_end(f); I != E; ++I) {
      if (auto *call = dyn_cast<CallBase>(&*I)) {
        auto targets = get_possible_call_targets(call);
        if (std::find(targets.begin(), targets.end(), func) != targets.end()) {
          insert_tainted_value(call, TaintReason::CONTROL_FLOW);
        }
      }
    }
  }
}
void Precalculations::print_analysis_result_remarks() {

  for (auto v : tainted_values) {
    if (auto *inst = dyn_cast<Instruction>(v->v)) {
      if (v->reason & TaintReason::CONTROL_FLOW) {
        errs() << "need for control flow:\n";
        errs() << inst->getFunction()->getName() << "\n";
        inst->dump();
      }
      if (v->reason & TaintReason::COMPUTE_TAG) {
        errs() << "need for tag compute:\n";
        errs() << inst->getFunction()->getName() << "\n";
        inst->dump();
      }
      if (v->reason & TaintReason::COMPUTE_DEST) {
        errs() << "need for dest compute:\n";
        errs() << inst->getFunction()->getName() << "\n";
        inst->dump();
      }
      if (v->reason == TaintReason::OTHER) {
        errs() << "need for other reason:\n";
        errs() << inst->getFunction()->getName() << "\n";
        inst->dump();
      }
    }
  }

  debug_printings();
}

void Precalculations::debug_printings() {
  errs() << "ADDITIONAL DEBUG PRINTING\n";

  for (auto v : tainted_values) {
    if (auto call = dyn_cast<CallBase>(v->v)) {
      if (call->getCalledFunction() == mpi_func->mpi_Isend) {
        // call->dump();
        print_parents(v);

        return;
      }
    }
  }
}

llvm::Function *Precalculations::get_global_re_init_function() {
  auto implementation_specifics = ImplementationSpecifics::get_instance();

  auto *func = Function::Create(
      FunctionType::get(Type::getVoidTy(M.getContext()), false),
      llvm::GlobalValue::InternalLinkage, "re_init_globals", M);
  auto bb = BasicBlock::Create(M.getContext(), "", func, nullptr);
  assert(bb->isEntryBlock());
  IRBuilder<> builder(bb);

  for (auto &global : M.globals()) {
    // if Comm World is needed there is no need to initialize it again, it
    // cannot be modified comm World will have no ptr info and therefore is
    // Written to check cannot be made
    if (is_tainted(&global) &&
        &global != implementation_specifics->COMM_WORLD) {
      assert(global.getType()->isPointerTy());
      auto global_info = insert_tainted_value(&global); // does find only
      assert(global_info->ptr_info);
      if (global_info->ptr_info->isWrittenTo()) {
        builder.CreateStore(global.getInitializer(), &global);
      } // else no need to do anything as it is not changed (readonly)
      // at least not by tainted instructions
    }
  }

  builder.CreateRetVoid();

  return func;
}
void Precalculations::remove_tainted_value(
    const std::shared_ptr<TaintedValue> &value_info) {
  tainted_values.erase(value_info);
}

llvm::GlobalVariable *
VtableManager::get_vtable_from_ptr_user(llvm::User *vtable_value) {
  assert(isa<ConstantAggregate>(vtable_value) &&
         "non constant vtable for a class?");
  assert(vtable_value->hasOneUser() &&
         "What kind of vtable is defined multiple times?");
  auto *vtable_initializer = cast<Constant>(vtable_value);
  if (isa<ConstantArray>(vtable_value)) {
    vtable_initializer =
        dyn_cast<Constant>(vtable_value->getUniqueUndroppableUser());
  }
  assert(vtable_initializer && "Vtable with a non constant initializer?");
  assert(vtable_initializer->hasOneUser() &&
         "storing the vtable into several globals is not supported?");
  auto *vtable_global =
      dyn_cast<Value>(vtable_initializer->getUniqueUndroppableUser());

  // errs() << "Found Vtable:\n";
  // vtable_global->dump();
  if (not isa<GlobalValue>(vtable_global)) {

    auto *current_level_of_definition = cast<Constant>(vtable_global);
    while (not isa<GlobalValue>(current_level_of_definition)) {
      assert(current_level_of_definition->hasOneUser() &&
             "storing the vtable into several globals is not supported?");
      current_level_of_definition = cast<Constant>(
          current_level_of_definition->getUniqueUndroppableUser());
    }
    // these special llvm magic globals are not touched
    // we will just use the normal initializers
    // as everything of this will be called before main actually starts
    assert(current_level_of_definition->getName().equals("llvm.global_ctors") ||
           current_level_of_definition->getName().equals("llvm.global_dtors"));
    return nullptr;
  }

  assert(dyn_cast<GlobalVariable>(vtable_global) &&
         "Vtable is not defined as a global?");
  return dyn_cast<GlobalVariable>(vtable_global);
}

void VtableManager::register_function_copy(llvm::Function *old_F,
                                           llvm::Function *new_F) {
  old_new_func_map.insert(std::make_pair(old_F, new_F));
  new_funcs.insert(new_F);
}

void VtableManager::perform_vtable_change_in_copies() {
  std::map<GlobalValue *, GlobalValue *> old_new_vtable_map;

  // collect all the vtables that need some changes
  for (auto pair : old_new_func_map) {

    auto old_func = pair.first;
    auto new_func = pair.second;

    for (auto u : old_func->users()) {
      if (isa<ConstantAggregate>(u)) {
        auto vtable_global = get_vtable_from_ptr_user(u);
        auto *vtable_value = dyn_cast<Constant>(u);
        // if get_vtable_from_ptr_user returns nullptr
        // we found one of the llvm special "vtables" @llvm.global_ctors
        // we will not change this (the application needs the full
        // initialization not just the parts relevant for tag computing)
        if (vtable_global && old_new_vtable_map.find(vtable_global) ==
                                 old_new_vtable_map.end()) {
          old_new_vtable_map.insert(
              // get the new vtable
              std::make_pair(vtable_global, get_replaced_vtable(vtable_value)));
        } else {
          // nothing to do, the first call will generate the full replaced
          // vtable, even if multiple entries in it needs to be replaced
        }
      }
    }
  }

  for (auto pair : old_new_vtable_map) {
    auto vtable_global_old = pair.first;
    auto *vtable_global_new = pair.second;

    // collections of uses to replace
    std::vector<std::tuple<Instruction *, Value *, Value *>> to_replace;
    for (auto *u : vtable_global_old->users()) {
      // only replace if in copy
      if (auto *inst = dyn_cast<Instruction>(u)) {
        if (new_funcs.find(inst->getFunction()) != new_funcs.end()) {
          to_replace.push_back(
              std::make_tuple(inst, vtable_global_old, vtable_global_new));
        }
      } else if (auto *constant = dyn_cast<ConstantExpr>(u)) {
        //  a use of a vtable entry
        // we need to replace the first operand with the new vtable, all other
        // operands stay the same
        std::vector<Constant *> operands;
        for (auto &op : constant->operands()) {
          operands.push_back(cast<Constant>(&op));
        }
        assert(operands[0] == vtable_global_old);
        operands[0] = vtable_global_new;
        Value *getelemptr_copy = constant->getWithOperands(operands);
        // all usages of this in copied functions
        for (auto uu : constant->users()) {
          if (auto *inst_uu = dyn_cast<Instruction>(uu)) {
            if (new_funcs.find(inst_uu->getFunction()) != new_funcs.end()) {
              to_replace.push_back(
                  std::make_tuple(inst_uu, constant, getelemptr_copy));
            }
          } else {
            errs() << "unknown use of vtable access :\n";
            u->dump();
            uu->dump();
            assert(false);
          }
        }
      } else {
        errs() << "unknown use of vtable:\n";
        u->dump();
        assert(false);
      }
    }
    // and replace
    for (auto triple : to_replace) {
      auto *inst = std::get<0>(triple);
      auto *from = std::get<1>(triple);
      auto *to = std::get<2>(triple);
      inst->replaceUsesOfWith(from, to);
      inst->dump();
    }

  } // end for each vtable
    // assert(false && "DEBUG");
}
GlobalVariable *
VtableManager::get_replaced_vtable(llvm::User *vtable_value_as_use) {
  auto vtable_global = get_vtable_from_ptr_user(vtable_value_as_use);

  auto *vtable_value = dyn_cast<ConstantArray>(vtable_value_as_use);
  assert(vtable_value && "The vtable is not a constant Array?");

  std::string name_of_copy =
      vtable_global->getName().str() + "_copy_for_precalc";

  // copy the vtable
  // auto *new_vtable_global = cast<GlobalVariable>(
  //    M.getOrInsertGlobal(name_of_copy, vtable_global->getType()));
  // new_vtable_global->setInitializer(nullptr);

  std::vector<Constant *> new_vtable;
  errs() << "old vtable:\n";
  vtable_global->dump();
  vtable_global->getType()->dump();

  for (unsigned int i = 0; i < vtable_value->getNumOperands(); ++i) {
    auto vtable_entry = vtable_value->getOperand(i);
    if (auto *func = dyn_cast<Function>(vtable_entry)) {
      if (old_new_func_map.find(func) != old_new_func_map.end()) {
        new_vtable.push_back(old_new_func_map[func]);
      } else {
        // this function was not copied: it will not be called during precalc
        new_vtable.push_back(Constant::getNullValue(vtable_entry->getType()));
      }
    } else {
      // keep old value (may be null or a special pure virtual value)
      new_vtable.push_back(vtable_entry);
    }
  }

  auto *new_vtable_value =
      ConstantArray::get(vtable_value->getType(), new_vtable);
  auto *old_vtable_initializer =
      cast<ConstantStruct>(vtable_value->getUniqueUndroppableUser());
  assert(old_vtable_initializer->getType()->getNumElements() ==
         1); // only the vtable array itself is the element
  auto *new_vtable_initializer =
      ConstantStruct::get(old_vtable_initializer->getType(), new_vtable_value);

  auto *new_vtable_global = new GlobalVariable(
      M, new_vtable_initializer->getType(), true, vtable_global->getLinkage(),
      new_vtable_initializer, name_of_copy, vtable_global);

  new_vtable_global->copyAttributesFrom(vtable_global);

  errs() << "new vtable:\n";
  new_vtable_global->dump();
  new_vtable_global->getType()->dump();

  return new_vtable_global;
}
