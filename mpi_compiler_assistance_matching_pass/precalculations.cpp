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
#include "mpi_functions.h"
#include "precalculation.h"

#include "implementation_specific.h"
#include "mpi_functions.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "debug.h"
using namespace llvm;

void Precalculations::add_precalculations(
    const std::vector<llvm::CallBase *> &to_precompute) {
  to_replace_with_envelope_register = to_precompute;

  for (auto call : to_precompute) {
    bool is_send = call->getCalledFunction() == mpi_func->mpi_send_init;
    auto *tag = get_tag_value(call, is_send);
    auto *src = get_src_value(call, is_send);

    insert_tainted_value(tag);
    insert_tainted_value(src);
    // TODO precompute comm as well?
    insert_tainted_value(call);
    visited_values.insert(call);
  }

  find_all_tainted_vals();
  for (const auto &f : functions_to_include) {
    f->initialize_copy();
  }
  // dont fuse this loops! we first need to initialize the copy before changing
  // calls
  for (const auto &f : functions_to_include) {
    replace_calls_in_copy(f);
  }

  for (const auto &f : functions_to_include) {
    f->prune_copy(tainted_values);
  }
  add_call_to_precalculation_to_main();
}

void Precalculations::find_all_tainted_vals() {
  while (tainted_values.size() > visited_values.size()) {
    std::set<Value *> not_visited;
    std::set_difference(tainted_values.begin(), tainted_values.end(),
                        visited_values.begin(), visited_values.end(),
                        std::inserter(not_visited, not_visited.begin()));

    for (auto v : not_visited) {
      visit_val(v);
    }
  }

  std::set<Value *> not_visited_assert;
  std::set_difference(
      tainted_values.begin(), tainted_values.end(), visited_values.begin(),
      visited_values.end(),
      std::inserter(not_visited_assert, not_visited_assert.begin()));
  assert(not_visited_assert.empty());
  // taint all the blocks that are relevant
  std::transform(tainted_values.begin(), tainted_values.end(),
                 std::inserter(tainted_blocks, tainted_blocks.begin()),
                 [](auto v) -> BasicBlock * {
                   if (auto *inst = dyn_cast<Instruction>(v))
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

void Precalculations::visit_val(llvm::Value *v) {
  errs() << "Visit\n";
  v->dump();
  visited_values.insert(v);

  if (auto *c = dyn_cast<Constant>(v)) {
    return;
  }
  if (auto *l = dyn_cast<LoadInst>(v)) {
    insert_tainted_value(l->getPointerOperand());
    if (l->getType()->isPointerTy()) {
      visit_ptr(l);
    }
    return;
  }
  if (auto *a = dyn_cast<AllocaInst>(v)) {
    visit_val(a);
    return;
  }
  if (auto *s = dyn_cast<StoreInst>(v)) {
    visit_val(s);
    return;
  }
  if (auto *op = dyn_cast<BinaryOperator>(v)) {
    // arithmetic
    // TODO do we need to exclude some opcodes?
    assert(op->getNumOperands() == 2);
    insert_tainted_value(op->getOperand(0));
    insert_tainted_value(op->getOperand(1));
    return;
  }
  if (auto *op = dyn_cast<CmpInst>(v)) {
    // cmp
    assert(op->getNumOperands() == 2);
    insert_tainted_value(op->getOperand(0));
    insert_tainted_value(op->getOperand(1));
    return;
  }
  if (auto *select = dyn_cast<SelectInst>(v)) {
    insert_tainted_value(select->getCondition());
    insert_tainted_value(select->getTrueValue());
    insert_tainted_value(select->getFalseValue());
    return;
  }
  if (auto *arg = dyn_cast<Argument>(v)) {
    visit_val(arg);
    return;
  }
  if (auto *call = dyn_cast<CallBase>(v)) {
    visit_val(call);
    return;
  }
  if (auto *phi = dyn_cast<PHINode>(v)) {
    visit_val(phi);
    return;
  }
  if (auto *gep = dyn_cast<GetElementPtrInst>(v)) {
    for (unsigned int i = 0; i < gep->getNumOperands(); ++i) {
      insert_tainted_value(gep->getOperand(i));
    }
    return;
  }

  errs() << "Support for analyzing this Value is not implemented yet\n";
  v->dump();
  assert(false);
}

void Precalculations::visit_ptr(llvm::Value *ptr) {
  insert_tainted_value(ptr);
  visited_values.insert(ptr);
  assert(ptr->getType()->isPointerTy());
  for (auto u : ptr->users()) {
    if (auto *s = dyn_cast<StoreInst>(u)) {
      insert_tainted_value(s);
      if (s->getValueOperand() == ptr) {
        visit_ptr(s->getPointerOperand());
      }
      continue;
    }
    if (auto *l = dyn_cast<LoadInst>(u)) {
      if (l->getType()->isPointerTy()) {
        // if a ptr is loaded we need to trace its usages
        insert_tainted_value(l);
      } // else no need to take care about this
      continue;
    }
    if (auto *call = dyn_cast<CallBase>(u)) {
      visit_call_from_ptr(call, ptr);
      continue;
    }
    if (auto *gep = dyn_cast<GetElementPtrInst>(u)) {
      visit_ptr(gep);
      continue;
    }
    if (auto *compare = dyn_cast<ICmpInst>(u)) {
      // nothing to do
      // the compare will be tainted if it is necessary for control flow else we
      // can ignore it
      continue;
    }
    errs() << "Support for analyzing this Value is not implemented yet\n";
    u->dump();
    assert(false);
  }
}

void Precalculations::visit_val(llvm::AllocaInst *alloca) {
  assert(visited_values.find(alloca) != visited_values.end());
  visit_ptr(alloca);
}

void Precalculations::visit_val(llvm::PHINode *phi) {
  assert(visited_values.find(phi) != visited_values.end());

  for (unsigned int i = 0; i < phi->getNumOperands(); ++i) {
    auto v = phi->getIncomingValue(i);
    tainted_values.insert(v);
  }
}

void Precalculations::visit_val(llvm::StoreInst *store) {
  assert(visited_values.find(store) != visited_values.end());

  assert(tainted_values.find(store->getPointerOperand()) !=
         tainted_values.end());
  insert_tainted_value(store->getValueOperand());
}

std::shared_ptr<FunctionToPrecalculate>
Precalculations::insert_functions_to_include(llvm::Function *func) {

  auto pos =
      std::find_if(functions_to_include.begin(), functions_to_include.end(),
                   [&func](const auto p) { return p->F_orig == func; });

  if (pos == functions_to_include.end()) {
    auto fun_to_precalc = std::make_shared<FunctionToPrecalculate>(func);
    functions_to_include.insert(fun_to_precalc);
    for (auto u : func->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        errs() << "Visit\n";
        call->dump();
        insert_tainted_value(call);
        visited_values.insert(call); // no need to do something with it just
                                     // make shure it is included
        continue;
      }
      /*
            errs() << "Error analyzing usage of function: " << func->getName()
                   << "\n";
            errs() << "Support for analyzing this Value is not implemented
         yet\n"; u->dump(); assert(false);
            */
      // uses that will result in indirect calls will be handled from the call
      // site (see visit_all_indirect_calls_for_FnType)
    }

    if (fn_types_with_indirect_calls.find(func->getFunctionType()) !=
        fn_types_with_indirect_calls.end()) {
      // this func may be called indirect we need to also include all other
      // funcs with the same type
      visit_all_indirect_calls_for_FnType(func->getFunctionType());
    }

    return fun_to_precalc;
  } else {
    return *pos;
  }
}

void Precalculations::visit_val(llvm::Argument *arg) {
  assert(visited_values.find(arg) != visited_values.end());

  auto *func = arg->getParent();

  auto fun_to_precalc = insert_functions_to_include(func);

  if (fun_to_precalc->args_to_use.find(arg->getArgNo()) ==
      fun_to_precalc->args_to_use.end()) {
    // else: nothing to do, this was already visited
    for (auto u : func->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        auto *operand = call->getArgOperand(arg->getArgNo());
        insert_tainted_value(operand);
        continue;
      }
    }
    fun_to_precalc->args_to_use.insert(arg->getArgNo());
    if (fn_types_with_indirect_calls.find(func->getFunctionType()) !=
        fn_types_with_indirect_calls.end()) {
      // this func may be called indirect we need to also include all other
      // funcs with the same type
      visit_all_indirect_call_args_for_FnType(func->getFunctionType(),
                                              arg->getArgNo());
    }
  }
}

void Precalculations::visit_val(llvm::CallBase *call) {
  assert(visited_values.find(call) != visited_values.end());

  // check if this is tainted as the ret val is used

  std::set<Value *> users_of_retval;
  std::transform(call->use_begin(), call->use_end(),
                 std::inserter(users_of_retval, users_of_retval.begin()),
                 [](const auto &u) { return dyn_cast<Value>(&u); });
  std::set<Value *> tainred_users_of_retval;
  std::set_union(
      users_of_retval.begin(), users_of_retval.end(), tainted_values.begin(),
      tainted_values.end(),
      std::inserter(tainred_users_of_retval, tainred_users_of_retval.begin()));
  bool need_return_val = not tainred_users_of_retval.empty();

  if (need_return_val) {
    assert(not call->isIndirectCall() && "TODO not implemented\n");
    // TODO
    for (auto bb = call->getCalledFunction()->begin();
         bb != call->getCalledFunction()->end(); ++bb) {
      if (auto *ret = dyn_cast<ReturnInst>(bb->getTerminator())) {
        insert_tainted_value(ret);
      }
    }
  }

  // we need to check the control flow if a exception is raised
  // if there are no resumes in called function: no need to do anything as an
  // exception cannot be raised
  if (auto *invoke = dyn_cast<InvokeInst>(call)) {
    assert(not call->isIndirectCall() && "TODO not implemented\n");
    // TODO
    for (auto bb = call->getCalledFunction()->begin();
         bb != call->getCalledFunction()->end(); ++bb) {
      if (auto *res = dyn_cast<ResumeInst>(bb->getTerminator())) {
        insert_tainted_value(res);
      }
    }
  }
}

void Precalculations::visit_call_from_ptr(llvm::CallBase *call,
                                          llvm::Value *ptr) {
  assert(visited_values.find(ptr) != visited_values.end());

  if (call->getCalledOperand() == ptr) {
    // visit from the function ptr: nothing to check
    return;
  }

  errs() << "Visit\n";
  call->dump();

  std::set<unsigned int> ptr_given_as_arg;
  for (unsigned int i = 0; i < call->arg_size(); ++i) {
    if (call->getArgOperand(i) == ptr) {
      ptr_given_as_arg.insert(i);
    }
  }
  assert(not ptr_given_as_arg.empty());

  auto *func = call->getCalledFunction();

  if (func == mpi_func->mpi_comm_size || func == mpi_func->mpi_comm_rank) {
    // we know it is safe to execute these "readonly" funcs
    if (*ptr_given_as_arg.begin() == 0 && ptr_given_as_arg.size() == 1) {
      // nothing to do only reads the communicator
    } else {
      // the needed value is the result of reading the comm
      assert(*ptr_given_as_arg.begin() == 1 && ptr_given_as_arg.size() == 1);
      visited_values.insert(call);
      insert_tainted_value(call);
      insert_tainted_value(
          call->getArgOperand(0)); // we also need to keep the comm
    }
    return;
  }
  if (func == mpi_func->mpi_init || func == mpi_func->mpi_init_thread) {
    // skip: MPI_Init will only transfer the cmd line args to all processes, not
    // modify them otherwise
    return;
  }
  if (func == mpi_func->mpi_send_init || func == mpi_func->mpi_recv_init) {
    // skip: these functions will be managed seperately anyway
    // it may be the case, that e.g. the buffer or request aliases with
    // something important
    return;
  }

  if (func->isIntrinsic() &&
      (func->getIntrinsicID() == Intrinsic::lifetime_start ||
       func->getIntrinsicID() == Intrinsic::lifetime_end)) {
    // ignore lifetime intrinsics
    return;
  }

  for (auto arg_num : ptr_given_as_arg) {
    auto *arg = func->getArg(arg_num);
    if (arg->hasAttribute(Attribute::NoCapture) &&
        arg->hasAttribute(Attribute::ReadOnly)) {
      continue; // nothing to do reading the val is allowed
      // TODO has foo( int ** array){ array[0][0]=0;} also readonly? as first
      // ptr lvl is only read
    }
    if (func->isDeclaration()) {
      errs() << "Can not analyze usage of external function:\n";
      func->dump();
      assert(false);
    } else {
      insert_tainted_value(arg);
      visited_values.insert(arg);
      visit_ptr(arg);
    }
  }
}

void Precalculations::insert_tainted_value(llvm::Value *v) {
  if (tainted_values.insert(v).second) {
    // only if not already in set
    if (auto *inst = dyn_cast<Instruction>(v)) {
      auto bb = inst->getParent();
      if (not bb->isEntryBlock()) {
        // we need to insert the instruction that lets the control flow go here
        for (auto pred_bb : predecessors(bb)) {
          auto *term = pred_bb->getTerminator();
          if (term->getNumSuccessors() > 1) {
            if (auto *branch = dyn_cast<BranchInst>(term)) {
              assert(branch->isConditional());
              insert_tainted_value(branch->getCondition());
              insert_tainted_value(branch);
              visited_values.insert(branch);
            } else if (auto *invoke = dyn_cast<InvokeInst>(term)) {
              insert_tainted_value(invoke);
              // we will later visit it
            } else {
              errs() << "Error analyzing CFG:\n";
              term->dump();
              assert(false);
            }
          } else {
            assert(dyn_cast<BranchInst>(term));
            assert(dyn_cast<BranchInst>(term)->isConditional() == false);
            assert(term->getSuccessor(0) == bb);
            visited_values.insert(term);
            insert_tainted_value(term);
          }
        }
      } else {
        // BB is function entry block
        insert_functions_to_include(bb->getParent());
      }
    }
  }
}

void FunctionToPrecalculate::initialize_copy() {
  assert(F_copy == nullptr);
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
        if (call->isIndirectCall() &&
            fn_types_with_indirect_calls.find(call->getFunctionType()) !=
                fn_types_with_indirect_calls.end()) {
          to_replace.push_back(call);
        } else {
          // callee not in functions_to_include, we will later remove it
          assert(tainted_values.find(callee) == tainted_values.end());
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
      if (tainted_values.find(orig_call->getArgOperand(i)) ==
          tainted_values.end()) {
        // set unused arg to 0 (so we dont need to compute it)
        call->setArgOperand(
            i, Constant::getNullValue(call->getArgOperand(i)->getType()));
      } // else pass arg normally
    }
  }

  replace_usages_of_func_in_copy(func);
}

// remove all unnecessary instruction
void FunctionToPrecalculate::prune_copy(
    const std::set<llvm::Value *> &tainted_values) {

  std::vector<Instruction *> to_prune;

  // first  gather all instructions, so that the iterator does not get broken if
  // we remove// stuff

  for (auto I = inst_begin(F_copy), E = inst_end(F_copy); I != E; ++I) {

    Instruction *inst = &*I;
    auto old_v = new_to_old_map[inst];
    if (tainted_values.find(old_v) == tainted_values.end()) {
      to_prune.push_back(inst);
    }
  }

  // remove stuff
  for (auto inst : to_prune) {

    if (inst->isTerminator()) {
      // if this terminator was not tainted: we can immediately return from this
      // function
      IRBuilder<> builder = IRBuilder<>(inst);
      if (inst->getFunction()->getReturnType()->isVoidTy()) {
        builder.CreateRetVoid();
      } else {
        builder.CreateRet(
            Constant::getNullValue(inst->getFunction()->getReturnType()));
      }
    }
    // we keep the exception handling instructions so that the module is still
    // correct if they are not tainted and an exception occurs we abort anyway
    // (otherwise we would have tainted the exception handling code)
    if (auto lp = dyn_cast<LandingPadInst>(inst)) {
      // lp->setCleanup(false);
      lp->dump();
    } else {
      inst->eraseFromParent();
    }
  }
  /*
    // and erase
    for (auto inst : to_prune) {
      inst->eraseFromParent();
    }
    */
  // TODO remove all unreachable blocks that may be left over
  // this should be done later by a dead Code elimination pass
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

  // search for MPI_init orInit Thread as precalc may only take place after that
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

void Precalculations::find_functionTypes_called_indirect() {

  for (auto &f : M.functions()) {
    for (auto I = inst_begin(f), E = inst_end(f); I != E; ++I) {

      Instruction *inst = &*I;
      if (auto call = dyn_cast<CallBase>(inst)) {
        if (call->isIndirectCall()) {
          // we do need to visit all indirect calls
          if (fn_types_with_indirect_calls.insert(call->getFunctionType())
                  .second) {
            // only if not already in set
            // errs() << "Function Type that needs to be visited:\n";
            // call->getFunctionType()->dump();
          }
        }
      }
    }
  }
}

void Precalculations::visit_all_indirect_calls_for_FnType(
    llvm::FunctionType *fntype) {
  for (auto &f : M.functions()) {
    if (f.getFunctionType() == fntype) {
      insert_functions_to_include(&f);
    }
    for (auto I = inst_begin(f), E = inst_end(f); I != E; ++I) {

      Instruction *inst = &*I;
      if (auto call = dyn_cast<CallBase>(inst)) {
        if (call->isIndirectCall()) {
          if (call->getFunctionType() == fntype) {
            insert_tainted_value(call);
            visited_values.insert(call);
            insert_tainted_value(call->getCalledOperand());
          }
        }
      }
    }
  }
}

void Precalculations::visit_all_indirect_call_args_for_FnType(
    llvm::FunctionType *fntype, unsigned int argNo) {

  for (auto &f : M.functions()) {
    for (auto I = inst_begin(f), E = inst_end(f); I != E; ++I) {

      Instruction *inst = &*I;
      if (auto call = dyn_cast<CallBase>(inst)) {
        if (call->isIndirectCall()) {
          if (call->getFunctionType() == fntype) {
            insert_tainted_value(call->getArgOperand(argNo));
          }
        }
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
    if (isa<ArrayType>(u->getType())) {
      // an array of function ptrs  e.g. a vtable for objects

      auto *val = dyn_cast<Value>(u);
      assert(val);

      errs() << "VTABLE Currently not implemented:\n";
      for (auto &uu : val->uses()) {
        uu->dump();
      }
      for (auto uu : val->users()) {
        uu->dump();
      }
      assert(false);

      continue;
    }
    errs() << "This usage is currently not supported:\n";
    u->dump();
    assert(false);
  }

  for (auto *inst : instructions_to_change) {
    bool has_replaced = inst->replaceUsesOfWith(func->F_orig, func->F_copy);
    assert(has_replaced);
  }
}