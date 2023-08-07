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

#include "conflict_detection.h"
#include "analysis_results.h"
#include "frontend_plugin_data.h"
#include "implementation_specific.h"
#include "mpi_functions.h"
#include "replacement.h"
#include "llvm/IR/Constants.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"

#include "debug.h"

using namespace llvm;

bool can_prove_val_different_with_scalarEvolution(Value *val_a, Value *val_b) {

  auto *inst_a = dyn_cast<Instruction>(val_a);
  auto *inst_b = dyn_cast<Instruction>(val_b);
  // swap args so that val_a is an instruction
  if (!inst_a && inst_b) {
    return can_prove_val_different_with_scalarEvolution(val_b, val_a);
  } else if (!inst_a && !inst_b) {
    return false;
  }
  assert(inst_a);

  ScalarEvolution *se = analysis_results->getSE(*inst_a->getFunction());
  assert(se != nullptr);

  if (!se->isSCEVable(inst_a->getType())) {
    return false;
  }

  // Debug(errs() << "try to prove difference within loop\n";)

  auto *sc_a = se->getSCEV(val_a);
  auto *sc_b = se->getSCEV(val_b);

  // Debug(sc_a->print(errs()); errs() << "\n"; sc_b->print(errs());errs() <<
  // "\n";)

  bool result = se->isKnownPredicate(CmpInst::Predicate::ICMP_NE, sc_a, sc_b);
  Debug(
      if (result) { errs() << "Known different\n"; } else {
        errs() << "could not prove difference\n";
      })

      return result;
}

// 1: proven different
// 0: could not prove difference
// -1: could actually prove identitiy
int can_prove_val_different(Value *val_a, Value *val_b) {

  errs() << "Comparing: \n";
  val_a->dump();
  val_b->dump();

  if (val_a->getType() != val_b->getType()) {
    // this should not happen anyway
    assert(false && "Trying comparing values of different types.");
    return true;
  }

  if (auto *c1 = dyn_cast<Constant>(val_a)) {
    if (auto *c2 = dyn_cast<Constant>(val_b)) {
      if (c1 != c2) {
        // different constants
        errs() << "Different Constants\n";
        return 1;
      } else {
        // proven same
        errs() << "SAME Constants\n";
        return -1;
      }
    }
  }

  if (can_prove_val_different_with_scalarEvolution(val_a, val_b)) {
    return 1;
  }

  // could not prove difference
  return 0;
}

Value *get_communicator_value(CallBase *mpi_call) {

  unsigned int total_num_args = 0;
  unsigned int communicator_arg_pos = 0;

  if (mpi_call->getCalledFunction() == mpi_func->mpi_send ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Bsend ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Ssend ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Rsend) {
    total_num_args = 6;
    communicator_arg_pos = 5;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_Isend) {
    total_num_args = 7;
    communicator_arg_pos = 5;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_recv ||
             mpi_call->getCalledFunction() == mpi_func->mpi_Irecv) {
    total_num_args = 7;
    communicator_arg_pos = 5;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_Sendrecv) {
    total_num_args = 12;
    communicator_arg_pos = 10;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_send_init ||
             mpi_call->getCalledFunction() == mpi_func->mpi_recv_init) {
    total_num_args = 7;
    communicator_arg_pos = 5;
  } else {
    errs() << mpi_call->getCalledFunction()->getName()
           << ": This MPI function is currently not supported\n";
    assert(false);
  }

  assert(mpi_call->arg_size() == total_num_args);

  return mpi_call->getArgOperand(communicator_arg_pos);
}

Value *get_src_value(CallBase *mpi_call, bool is_send) {

  unsigned int total_num_args = 0;
  unsigned int src_arg_pos = 0;

  if (mpi_call->getCalledFunction() == mpi_func->mpi_send ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Bsend ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Ssend ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Rsend) {
    assert(is_send);
    total_num_args = 6;
    src_arg_pos = 3;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_Isend) {
    assert(is_send);
    total_num_args = 7;
    src_arg_pos = 3;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_recv ||
             mpi_call->getCalledFunction() == mpi_func->mpi_Irecv) {
    assert(!is_send);
    total_num_args = 7;
    src_arg_pos = 3;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_Sendrecv) {
    total_num_args = 12;
    if (is_send)
      src_arg_pos = 3;
    else
      src_arg_pos = 8;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_send_init ||
             mpi_call->getCalledFunction() == mpi_func->mpi_recv_init) {
    total_num_args = 7;
    src_arg_pos = 3;
  } else {
    errs() << mpi_call->getCalledFunction()->getName()
           << ": This MPI function is currently not supported\n";
    assert(false);
  }

  assert(mpi_call->arg_size() == total_num_args);

  return mpi_call->getArgOperand(src_arg_pos);
}

Value *get_tag_value(CallBase *mpi_call, bool is_send) {

  unsigned int total_num_args = 0;
  unsigned int tag_arg_pos = 0;

  if (mpi_call->getCalledFunction() == mpi_func->mpi_send ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Bsend ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Ssend ||
      mpi_call->getCalledFunction() == mpi_func->mpi_Rsend) {
    assert(is_send);
    total_num_args = 6;
    tag_arg_pos = 4;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_Isend) {
    assert(is_send);
    total_num_args = 7;
    tag_arg_pos = 4;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_recv ||
             mpi_call->getCalledFunction() == mpi_func->mpi_Irecv) {
    assert(!is_send);
    total_num_args = 7;
    tag_arg_pos = 4;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_Sendrecv) {
    total_num_args = 12;
    if (is_send)
      tag_arg_pos = 4;
    else
      tag_arg_pos = 9;
  } else if (mpi_call->getCalledFunction() == mpi_func->mpi_send_init ||
             mpi_call->getCalledFunction() == mpi_func->mpi_recv_init) {
    total_num_args = 7;
    tag_arg_pos = 4;
  } else {
    errs() << mpi_call->getCalledFunction()->getName()
           << ": This MPI function is currently not supported\n";
    assert(false);
  }

  assert(mpi_call->arg_size() == total_num_args);

  return mpi_call->getArgOperand(tag_arg_pos);
}

// true if a is known to be before b, false otherwise
bool known_to_be_before(Instruction *a, Instruction *b) {

  if (a->getFunction() != b->getFunction()) {
    return false;
  }

  auto *domtree = analysis_results->getDomTree(*a->getFunction());

  return domtree->dominates(a, b);
}

Value *get_runtime_value_global(Instruction *insert_point, LoadInst *v) {

  return nullptr;
}

Value *get_runtime_value_local(Instruction *insert_point, LoadInst *v) {

  auto *ptr = dyn_cast<AllocaInst>(v->getPointerOperand());
  assert(ptr);

  if (insert_point->getFunction() != ptr->getFunction()) {
    // only inside one function
    return nullptr;
  }
  if (known_to_be_before(v, insert_point)) {
    return v;
  }

  // TODO this is the interesting part as we need to have the value of v to be
  // available before it is computed

  return nullptr;
}

// try to get the runtime value of the loadInst at the specified insert point
// returns nullptr if it connot be determined
Value *get_runtime_value(Instruction *insert_point, LoadInst *v) {
  if (auto *global = dyn_cast<GlobalVariable>(v->getPointerOperand())) {
    get_runtime_value_global(insert_point, v);
  }
  if (auto *local = dyn_cast<AllocaInst>(v->getPointerOperand())) {
    get_runtime_value_local(insert_point, v);
  }

  return nullptr;
}

Value *get_runtime_value(Instruction *insert_point, Constant *c) { return c; }

Value *get_runtime_value(Instruction *insert_point, Value *v) {

  if (auto *c = dyn_cast<Constant>(v)) {
    return get_runtime_value(insert_point, c);
  }
  if (auto *l = dyn_cast<LoadInst>(v)) {
    return get_runtime_value(insert_point, l);
  }

  // are there other cases we can handle?
  // PHI, select, arithmetic

  return nullptr;
}

// TODO implement
// TODO refactoring: extra file?
// i8 Value with result of runtime check
// returns nullptr if runtime check is not possible
Value *get_runtime_check(Instruction *insert_point, Value *val_a,
                         Value *val_b) {
  assert(val_a->getType() == val_b->getType());
  auto *inst_a = dyn_cast<Instruction>(val_a);
  auto *inst_b = dyn_cast<Instruction>(val_b);
  // swap args so that val_a is an instruction
  if (!inst_a && inst_b) {
    return get_runtime_check(insert_point, val_b, val_a);
  }
  assert(inst_a);

  auto *runtime_a = get_runtime_value(insert_point, val_a);
  auto *runtime_b = get_runtime_value(insert_point, val_b);

  if (runtime_a && runtime_b) {
    IRBuilder<> builder = IRBuilder<>(insert_point);
    // TODO does this work with ptr as well?
    return builder.CreateICmpNE(runtime_a, runtime_b);
  }

  errs() << "This instruction can not be analyzed: ";
  inst_a->dump();

  return ConstantInt::get(IntegerType::getInt8Ty(val_a->getContext()), 0);
}

std::shared_ptr<PersistentMPIInitCall>
PersistentMPIInitCall::get_PersistentMPIInitCall(llvm::CallBase *init_call) {

  // allow make_shared to call the private constructor
  struct make_shared_enabler : public PersistentMPIInitCall {
    make_shared_enabler(llvm::CallBase *init_call)
        : PersistentMPIInitCall(std::forward<llvm::CallBase *>(init_call)) {}
  };
  if (instances.find(init_call) != instances.end()) {
    return instances[init_call];
  } else {
    auto new_instance = std::make_shared<make_shared_enabler>(init_call);
    instances[init_call] = new_instance;
    new_instance->populate_conflicting_calls();
    return new_instance;
  }
}

std::map<llvm::CallBase *, std::shared_ptr<PersistentMPIInitCall>>
    PersistentMPIInitCall::instances = {};

PersistentMPIInitCall::PersistentMPIInitCall(llvm::CallBase *init_call)
    : init_call(init_call) {
  bool is_send = is_send_function(init_call->getCalledFunction());
  if (is_send) {
    assert(init_call->getCalledFunction() == mpi_func->mpi_send_init);
  } else {
    assert(init_call->getCalledFunction() == mpi_func->mpi_recv_init);
  }
  tag = get_tag_value(init_call, is_send);
  src = get_src_value(init_call, is_send);
  comm = get_communicator_value(init_call);
}

void PersistentMPIInitCall::populate_conflicting_calls() {
  auto frontend_plugin_data = FrontendPluginData::get_instance();
  for (auto c :
       frontend_plugin_data->get_possibly_conflicting_calls(init_call)) {
    conflicting_calls.push_back(
        PersistentMPIInitCall::get_PersistentMPIInitCall(c));
  }
}

void PersistentMPIInitCall::perform_replacement() {

  assert(replaced == false);

  std::vector<Value *> conf_res;

  std::transform(conflicting_calls.begin(), conflicting_calls.end(),
                 std::back_inserter(conf_res), [this](auto c) {
                   return c->get_conflict_result(this->shared_from_this());
                 });

  auto combined = combine_runtime_checks(init_call, conf_res);

  if (conflicting_calls.empty()) {
    // nothing to conflict with
    combined = get_runtime_check_result_true(init_call);
  }

  auto as_str = get_runtime_check_result_str(init_call, combined);

  bool is_send = is_send_function(init_call->getCalledFunction());
  if (is_send) {
    assert(init_call->getCalledFunction() == mpi_func->mpi_send_init);
    replace_init_call(init_call, mpi_func->optimized.mpi_send_init_info);
  } else {
    assert(init_call->getCalledFunction() == mpi_func->mpi_recv_init);
    replace_init_call(init_call, mpi_func->optimized.mpi_recv_init_info);
  }

  replaced = true;
}

llvm::Value *PersistentMPIInitCall::compute_conflict_result(
    const std::shared_ptr<PersistentMPIInitCall> &other) {

  assert(replaced == false);

  auto *other_tag = other->get_tag();
  auto *other_src = other->get_src();
  auto *other_comm = other->get_communicator();

  // check for wildcard usage
  auto mpi_implementation_specifics = ImplementationSpecifics::get_instance();
  if (src == mpi_implementation_specifics->ANY_SOURCE ||
      other_src == mpi_implementation_specifics->ANY_SOURCE ||
      tag == mpi_implementation_specifics->ANY_TAG ||
      other_tag == mpi_implementation_specifics->ANY_TAG) {
    // wildcard always conflicts
    return get_runtime_check_result_false(init_call);
  }

  // static check
  auto tag_different = can_prove_val_different(tag, other_tag);
  auto src_different = can_prove_val_different(src, other_src);
  auto comm_different = can_prove_val_different(comm, other_comm);

  if (tag_different == 1 || src_different == 1 || comm_different == 1) {
    return get_runtime_check_result_true(init_call);
  }

  CallBase *first_call = nullptr;
  auto order = FrontendPluginData::get_instance()->get_order(init_call,
                                                             other->init_call);
  if (order == Before || order == BeforeInLoop) {
    first_call = init_call;
  }
  if (order == After || order == AfterInLoop) {
    first_call = other->init_call;
  }
  if (first_call) {

    Value *tag_runtime_result = nullptr;
    if (tag_different == 0) {
      // otherwise static analysis has proven them to be same
      tag_runtime_result = get_runtime_check(first_call, tag, other_tag);
    }
    Value *src_runtime_result = nullptr;
    if (src_different == 0) {
      // otherwise static analysis has proven them to be same
      src_runtime_result = get_runtime_check(first_call, src, other_src);
    }
    Value *comm_runtime_result = nullptr;
    if (comm_different == 0) {
      // otherwise static analysis has proven them to be same
      comm_runtime_result = get_runtime_check(first_call, comm, other_comm);
    }

    return combine_runtime_checks(init_call, tag_runtime_result,
                                  src_runtime_result, comm_runtime_result);
  } // else no call was determined to be before the other: we dont know where to
  // insert the runtime check
  return get_runtime_check_result_false(init_call);
}

llvm::Value *PersistentMPIInitCall::get_conflict_result(
    const std::shared_ptr<PersistentMPIInitCall> &other) {

  if (conflict_results.find(other) != conflict_results.end()) {
    return conflict_results[other];
  }

  auto v = compute_conflict_result(other);
  conflict_results[other] = v;
  other->conflict_results[shared_from_this()] = v;
  return v;
}
