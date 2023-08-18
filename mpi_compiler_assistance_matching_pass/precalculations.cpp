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

// True if callee can raise an exception and the exception handling code is
// actually tainted if exception handling is not tainted, we dont need to handle
// the exception anyway and abortion is fine in this case
bool Precalculations::is_invoke_necessary_for_control_flow(
    llvm::InvokeInst *invoke) {

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
  return false;
}

bool is_allocation(llvm::CallBase *call) {
  // operator new
  if (call->getCalledFunction()->getName() == "_Znwm") {
    assert(isa<Constant>(call->getArgOperand(0)) &&
           "Non constant allocation in new??");
    return true;
  }
  return false;
}

bool is_free(Function *func) {
  // operator delete
  if (func->getName() == "_ZdlPv") {
    return true;
  }
  return false;
}

bool is_free(llvm::CallBase *call) {
  // operator delete
  if (call->getCalledFunction()->getName() == "_ZdlPv") {
    return true;
  }
  return false;
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
    visited_values.insert(new_val);
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
  while (tainted_values.size() > visited_values.size()) {
    std::set<std::shared_ptr<TaintedValue>> not_visited;
    std::set_difference(tainted_values.begin(), tainted_values.end(),
                        visited_values.begin(), visited_values.end(),
                        std::inserter(not_visited, not_visited.begin()));

    for (auto v : not_visited) {
      visit_val(v);
    }
  }
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

  auto pos2 = std::find_if(
      visited_values.begin(), visited_values.end(), [](auto v) -> bool {
        if (auto call = dyn_cast<CallBase>(v->v)) {
          if (call->getCalledFunction() &&
              (call->getCalledFunction() == mpi_func->mpi_init ||
               call->getCalledFunction() == mpi_func->mpi_init_thread)) {
            return true;
          }
        }
        return false;
      });

  visited_values.erase(*pos2);

  // done removing of MPI init

  // only for asserting that the subset is valid:
  std::set<std::shared_ptr<TaintedValue>> not_visited_assert;
  std::set_difference(
      tainted_values.begin(), tainted_values.end(), visited_values.begin(),
      visited_values.end(),
      std::inserter(not_visited_assert, not_visited_assert.begin()));
  assert(not_visited_assert.empty());
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

void Precalculations::visit_val(std::shared_ptr<TaintedValue> v) {
  errs() << "Visit\n";
  v->v->dump();
  visited_values.insert(v);

  if (v->is_pointer()) {
    visit_ptr(v);
  }

  if (auto *c = dyn_cast<Constant>(v->v)) {
    return;
  }
  if (auto *l = dyn_cast<LoadInst>(v->v)) {
    insert_tainted_value(l->getPointerOperand(), v);
    return;
  }
  if (auto *a = dyn_cast<AllocaInst>(v->v)) {
    // nothing to do: visit_ptr is called on all ptrs anyway
    return;
  }
  if (auto *s = dyn_cast<StoreInst>(v->v)) {
    if (is_tainted(s->getPointerOperand())) {
      // visit form ptr
      insert_tainted_value(s->getValueOperand(), v);
    } else {
      // capture ptr
      insert_tainted_value(s->getPointerOperand(), v);
    }
    return;
  }
  if (auto *op = dyn_cast<BinaryOperator>(v->v)) {
    // arithmetic
    // TODO do we need to exclude some opcodes?
    assert(op->getNumOperands() == 2);
    insert_tainted_value(op->getOperand(0), v);
    insert_tainted_value(op->getOperand(1), v);
    return;
  }
  if (auto *op = dyn_cast<CmpInst>(v->v)) {
    // cmp
    assert(op->getNumOperands() == 2);
    insert_tainted_value(op->getOperand(0), v);
    insert_tainted_value(op->getOperand(1), v);
    return;
  }
  if (auto *select = dyn_cast<SelectInst>(v->v)) {
    insert_tainted_value(select->getCondition(), v);
    insert_tainted_value(select->getTrueValue(), v);
    insert_tainted_value(select->getFalseValue(), v);
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
    for (unsigned int i = 0; i < phi->getNumOperands(); ++i) {
      auto vv = phi->getIncomingValue(i);
      insert_tainted_value(vv, v);
    }
    return;
  }
  if (auto *cast = dyn_cast<CastInst>(v->v)) {
    insert_tainted_value(cast->getOperand(0), v);
    return;
  }
  if (auto *gep = dyn_cast<GetElementPtrInst>(v->v)) {
    for (unsigned int i = 0; i < gep->getNumOperands(); ++i) {
      insert_tainted_value(gep->getOperand(i), v);
    }
    assert(is_tainted(gep->getPointerOperand()));
    return;
  }

  errs() << "Support for analyzing this Value is not implemented yet\n";
  v->v->dump();
  assert(false);
}

void Precalculations::visit_ptr(std::shared_ptr<TaintedValue> ptr) {
  assert(ptr->is_pointer());
  // insert_tainted_value(ptr->v, ptr);
  visited_values.insert(ptr);

  for (auto u : ptr->v->users()) {
    if (auto *s = dyn_cast<StoreInst>(u)) {
      auto new_val = insert_tainted_value(s, ptr);
      // all stores to this ptr or when the ptr is captured
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
      for (unsigned int i = 0; i < phi->getNumOperands(); ++i) {
        auto vv = phi->getIncomingValue(i);
        insert_tainted_value(vv, ptr);
      }
      insert_tainted_value(phi, ptr);
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
        visited_values.insert(new_val); // no need to do something with it
                                        // just make shure it is included
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

  auto *func = arg->getParent();

  auto fun_to_precalc = insert_functions_to_include(func);

  if (fun_to_precalc->args_to_use.find(arg->getArgNo()) ==
      fun_to_precalc->args_to_use.end()) {

    fun_to_precalc->args_to_use.insert(arg->getArgNo());
    // else: nothing to do, this was already visited
    for (auto u : func->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        auto *operand = call->getArgOperand(arg->getArgNo());
        insert_tainted_value(operand, arg_info);
        continue;
      }
      if (functions_that_may_be_called_indirect.find(func) !=
          functions_that_may_be_called_indirect.end()) {
        // this func may be called indirect we need to visit all indirect call
        // sites
        taint_all_indirect_call_args(func, arg->getArgNo());
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
  assert(is_visited(call));

  std::vector<Function *> possible_targets = get_possible_call_targets(call);

  bool need_return_val = is_retval_of_call_used(call);
  if (need_return_val) {
    if (is_allocation(call)) {
      // nothing to do, just keep this call around, it will later be replaced
      // TODO
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
    insert_tainted_value(call->getCalledOperand(), call_info);
  }
}

void Precalculations::visit_call_from_ptr(llvm::CallBase *call,
                                          std::shared_ptr<TaintedValue> ptr) {
  // TODO Implement with new info

  if (call->getCalledOperand() == ptr->v) {
    // visit from the function ptr: nothing to check
    return;
  }

  auto new_val = insert_tainted_value(call, ptr);
  visited_values.insert(new_val);

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
      } else {
        // the needed value is the result of reading the comm
        assert(*ptr_given_as_arg.begin() == 1 && ptr_given_as_arg.size() == 1);
        // TODO
        auto new_val = insert_tainted_value(call, ptr);
        visited_values.insert(new_val);

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
        continue; // nothing to do reading the val is allowed
        // TODO has foo( int ** array){ array[0][0]=0;} also readonly? as
        // first ptr lvl is only read
      }
      if (func->isDeclaration()) {
        errs() << "Can not analyze usage of external function:\n";
        func->dump();
        assert(false);
      } else {
        auto new_val = insert_tainted_value(arg, ptr);
        // TODO
        visited_values.insert(new_val);
        visit_ptr(new_val);
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
    // TODO do we need to also asses if a value is needed for tag and dest
    // compute?
    inserted_elem = std::make_shared<TaintedValue>(v);
    if (from != nullptr) {
      inserted_elem->reason = from->reason;
      inserted_elem->parents.insert(from);
      from->children.insert(inserted_elem);
    }
    tainted_values.insert(inserted_elem);
    if (auto *inst = dyn_cast<Instruction>(v)) {
      auto bb = inst->getParent();
      if (not bb->isEntryBlock()) {
        // we need to insert the instruction that lets the control flow go
        // here
        for (auto pred_bb : predecessors(bb)) {
          auto *term = pred_bb->getTerminator();
          if (term->getNumSuccessors() > 1) {
            if (auto *branch = dyn_cast<BranchInst>(term)) {
              assert(branch->isConditional());
              insert_tainted_value(branch->getCondition(),
                                   TaintReason::CONTROL_FLOW);
              auto new_val =
                  insert_tainted_value(branch, TaintReason::CONTROL_FLOW);
              visited_values.insert(new_val);
            } else if (auto *invoke = dyn_cast<InvokeInst>(term)) {
              insert_tainted_value(invoke, TaintReason::CONTROL_FLOW);
              // we will later visit it
            } else if (auto *switch_inst = dyn_cast<SwitchInst>(term)) {
              auto new_val =
                  insert_tainted_value(switch_inst, TaintReason::CONTROL_FLOW);
              visited_values.insert(new_val);
              insert_tainted_value(switch_inst->getCondition(),
                                   TaintReason::CONTROL_FLOW);
            } else {
              errs() << "Error analyzing CFG:\n";
              term->dump();
              assert(false);
            }
          } else {
            assert(dyn_cast<BranchInst>(term));
            assert(dyn_cast<BranchInst>(term)->isConditional() == false);
            assert(term->getSuccessor(0) == bb);
            auto new_val =
                insert_tainted_value(term, TaintReason::CONTROL_FLOW);
            visited_values.insert(new_val);
          }
        }
      } else {
        // BB is function entry block
        insert_functions_to_include(bb->getParent());
      }
    }
  } else {
    // the present value form the set
    inserted_elem = *std::find_if(tainted_values.begin(), tainted_values.end(),
                                  [&v](const auto &vv) { return vv->v == v; });
    if (from != nullptr) {
      inserted_elem->parents.insert(from);
      from->children.insert(inserted_elem);
      propergate_state_to_children(from);
    }
  }

  assert(inserted_elem != nullptr);
  if (inserted_elem->is_pointer()) {
    insert_tainted_ptr(inserted_elem, from);
  }
  return inserted_elem;
}

void add_gep_idx(std::shared_ptr<TaintedValue> new_ptr,
                 std::shared_ptr<TaintedValue> from, unsigned int idx_to_add) {

  if (from->whole_ptr_is_relevant == true) {
    new_ptr->whole_ptr_is_relevant = true;
    return;
  }

  for (auto idx_list : from->important_gep_index) {
    auto new_idx_list = std::make_shared<ImportantGEPIndex>();
    new_idx_list->important_gep_index.push_back(idx_to_add);
    for (auto idx : idx_list->important_gep_index) {
      new_idx_list->important_gep_index.push_back(idx);
    }
    new_ptr->important_gep_index.insert(new_idx_list);
  }

  if (from->important_gep_index.empty()) {
    auto new_idx_list = std::make_shared<ImportantGEPIndex>();
    new_idx_list->important_gep_index.push_back(idx_to_add);
    new_ptr->important_gep_index.insert(new_idx_list);
  }

  // TODO propergate this information to all childs of new_ptr if necessary
}

void Precalculations::insert_tainted_ptr(std::shared_ptr<TaintedValue> new_ptr,
                                         std::shared_ptr<TaintedValue> from) {
  if (from == nullptr || from->v == nullptr) {
    return;
  }
  assert(new_ptr->v->getType()->isPointerTy());
  if (new_ptr == from) {
    if (auto *alloc = dyn_cast<AllocaInst>(from->v)) {
      return;
    }
    if (auto *call = dyn_cast<CallBase>(from->v)) {
      assert(is_allocation(call));
      return;
    }
    // visiting the ptr allocation
  }
  assert(new_ptr != from);
  assert(new_ptr->v != from->v);

  errs() << "Visit Ptr:\n";
  new_ptr->v->dump();
  errs() << "from:\n";
  from->v->dump();

  if (auto *store = dyn_cast<StoreInst>(from->v)) {
    if (new_ptr->v == store->getPointerOperand()) {
      add_gep_idx(new_ptr, from, 0);
      errs() << "new idx:\n";
      for (auto idx_list : new_ptr->important_gep_index) {
        for (auto idx : idx_list->important_gep_index) {
          errs() << idx << " ";
        }
        errs() << "\n";
      }

      assert(false && "DEBUG ASSERTION");
    } else {
      // Noting to do if this ptr is stored
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
      // an invoke because it may return an exception
      // but it actually is exception free for our purpose
      if (not is_invoke_necessary_for_control_flow(invoke) &&
          not is_retval_of_call_used(invoke)) {
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

void Precalculations::taint_all_indirect_call_args(llvm::Function *func,
                                                   unsigned int ArgNo) {
  // TODO this could be done more efficient...
  for (auto &f : M.functions()) {
    for (auto I = inst_begin(f), E = inst_end(f); I != E; ++I) {
      if (auto *call = dyn_cast<CallBase>(&*I)) {
        auto targets = get_possible_call_targets(call);
        if (std::find(targets.begin(), targets.end(), func) != targets.end()) {
          insert_tainted_value(
              call->getArgOperand(ArgNo)); // TODO true false false
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
          insert_tainted_value(call); // TODO true false false
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
