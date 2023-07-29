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

    tainted_values.insert(tag);
    tainted_values.insert(src);
    // TODO precompute comm as well?
    tainted_values.insert(call);
    visited_values.insert(call);
  }

  find_all_tainted_vals();
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
}

void Precalculations::visit_val(llvm::Value *v) {
  errs() << "Visit\n";
  v->dump();
  visited_values.insert(v);

  if (auto *c = dyn_cast<Constant>(v)) {
    return;
  }
  if (auto *l = dyn_cast<LoadInst>(v)) {
    tainted_values.insert(l->getPointerOperand());
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
    tainted_values.insert(op->getOperand(0));
    tainted_values.insert(op->getOperand(1));
    return;
  }
  if (auto *arg = dyn_cast<Argument>(v)) {
    visit_val(arg);
    return;
  }

  errs() << "Support for analyzing this Value is not implemented yet\n";
  v->dump();
  assert(false);
}

void Precalculations::visit_val(llvm::AllocaInst *alloca) {
  assert(visited_values.find(alloca) != visited_values.end());

  for (auto u : alloca->users()) {
    if (auto *s = dyn_cast<StoreInst>(u)) {
      tainted_values.insert(s);
      continue;
    }
    if (auto *l = dyn_cast<LoadInst>(u)) {
      continue; // no need to take care about this
    }
    errs() << "Support for analyzing this Value is not implemented yet\n";
    u->dump();
    assert(false);
  }
}

void Precalculations::visit_val(llvm::StoreInst *store) {
  assert(visited_values.find(store) != visited_values.end());

  assert(tainted_values.find(store->getPointerOperand()) !=
         tainted_values.end());
  tainted_values.insert(store->getValueOperand());
}

void Precalculations::visit_val(llvm::Argument *arg) {
  assert(visited_values.find(arg) != visited_values.end());

  auto *func = arg->getParent();

  auto pos =
      std::find_if(functions_to_include.begin(), functions_to_include.end(),
                   [&func](const auto p) { return p->F_orig == func; });

  std::shared_ptr<FunctionToPrecalculate> fun_to_precalc = nullptr;
  if (pos == functions_to_include.end()) {
    fun_to_precalc = std::make_shared<FunctionToPrecalculate>(func);
    functions_to_include.insert(fun_to_precalc);
  } else {
    fun_to_precalc = *pos;
  }

  if (fun_to_precalc->args_to_use.find(arg->getArgNo()) ==
      fun_to_precalc->args_to_use.end()) {
    // else: nothing to do, this was already visited
    for (auto u : func->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        errs() << "Visit\n";
        call->dump();
        tainted_values.insert(call);
        visited_values.insert(call); // no need to do something with it just
                                     // make shure to visit the args
        auto *operand = call->getArgOperand(arg->getArgNo());
        tainted_values.insert(operand);
        continue;
      }
      errs() << "Support for analyzing this Value is not implemented yet\n";
      u->dump();
      assert(false);
    }
    fun_to_precalc->args_to_use.insert(arg->getArgNo());
  }
}