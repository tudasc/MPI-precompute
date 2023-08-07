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
  // dont fuse this loops we first need to initialize the copy before changing
  // calls
  for (const auto &f : functions_to_include) {
    replace_calls_in_copy(f);
  }
  // one MAY be able to fuse these 2 loops though
  // TODO evaluate if true
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

  errs() << "Support for analyzing this Value is not implemented yet\n";
  v->dump();
  assert(false);
}

void Precalculations::visit_ptr(llvm::Value *ptr) {
  assert(ptr->getType()->isPointerTy());
  for (auto u : ptr->users()) {
    if (auto *s = dyn_cast<StoreInst>(u)) {
      insert_tainted_value(s);
      continue;
    }
    if (auto *l = dyn_cast<LoadInst>(u)) {
      continue; // no need to take care about this
    }
    if (auto *call = dyn_cast<CallBase>(u)) {
      visit_call_from_ptr(call, ptr);
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
      errs() << "Support for analyzing this Value is not implemented yet\n";
      u->dump();
      assert(false);
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
      errs() << "Support for analyzing this Value is not implemented yet\n";
      u->dump();
      assert(false);
    }
    fun_to_precalc->args_to_use.insert(arg->getArgNo());
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
  assert(need_return_val);

  assert(call->getCalledFunction()->willReturn());
  for (auto bb = call->getCalledFunction()->begin();
       bb != call->getCalledFunction()->end(); ++bb) {
    if (auto *ret = dyn_cast<ReturnInst>(bb->getTerminator())) {
      insert_tainted_value(ret);
    }
  }
}

void Precalculations::visit_call_from_ptr(llvm::CallBase *call,
                                          llvm::Value *ptr) {
  assert(visited_values.find(ptr) != visited_values.end());

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
        for (auto pred_bb : predecessors(bb)) {
          auto *term = pred_bb->getTerminator();
          if (term->getNumSuccessors() > 1) {
            if (auto *branch = dyn_cast<BranchInst>(term)) {
              assert(branch->isConditional());
              insert_tainted_value(branch->getCondition());
              insert_tainted_value(branch);
              visited_values.insert(branch);
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
        // callee not in functions_to_include
        assert(tainted_values.find(callee) == tainted_values.end());
      }
    }
  }

  for (auto *call : to_replace) {
    auto callee = call->getCalledFunction();
    if (callee == mpi_func->mpi_send_init) {
      auto tag = get_tag_value(call, true);
      auto src = get_src_value(call, true);
      IRBuilder<> builder = IRBuilder<>(call);
      auto new_call =
          builder.CreateCall(mpi_func->optimized.register_send_tag, {src, tag});
      call->replaceAllUsesWith(new_call);
      call->eraseFromParent();

      continue;
    }
    if (callee == mpi_func->mpi_recv_init) {
      auto tag = get_tag_value(call, true);
      auto src = get_src_value(call, true);
      IRBuilder<> builder = IRBuilder<>(call);
      auto new_call =
          builder.CreateCall(mpi_func->optimized.register_recv_tag, {src, tag});
      call->replaceAllUsesWith(new_call);
      call->eraseFromParent();

      continue;
    }
    // end handling calls to MPI

    auto pos = std::find_if(functions_to_include.begin(),
                            functions_to_include.end(), [&call](const auto p) {
                              return p->F_orig == call->getCalledFunction();
                            });
    assert(pos != functions_to_include.end());

    const auto &function_information = *pos;

    call->setCalledFunction(function_information->F_copy);
    // null all not used args
    for (unsigned int i = 0; i < call->arg_size(); ++i) {
      if (function_information->args_to_use.find(i) ==
          function_information->args_to_use.end()) {
        call->setArgOperand(
            i, Constant::getNullValue(call->getArgOperand(i)->getType()));
      } // else pass arg normally
    }
  }
}

// remove all unnecessary instruction
void FunctionToPrecalculate::prune_copy(
    const std::set<llvm::Value *> &tainted_values) {

  // we need to get the reverse mapping
  std::map<Value *, Value *> new_to_old_map;

  for (auto v : old_new_map) {
    Value *old_v = const_cast<Value *>(v.first);
    Value *new_v = v.second;
    new_to_old_map.insert(std::make_pair(new_v, old_v));
  }

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
    inst->eraseFromParent();
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