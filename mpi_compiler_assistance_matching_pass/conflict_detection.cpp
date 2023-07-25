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

bool can_prove_val_different(Value *val_a, Value *val_b) {

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
        return true;
      } else {
        // proven same
        errs() << "SAME Constants\n";
        return false;
      }
    }
  }

  if (can_prove_val_different_with_scalarEvolution(val_a, val_b)) {
    return true;
  }

  // could not prove difference
  return false;
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

// TODO implement
bool is_runtime_check_possible(Value *val_a, Value *val_b) {
  assert(false && "Not implemented");
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
  bool perform_replacement = true;
  auto mpi_implementation_specifics = ImplementationSpecifics::get_instance();
  for (auto c : conflicting_calls) {

    auto *comm1 = comm;
    auto *comm2 = c->get_communicator();
    if (can_prove_val_different(comm1, comm2)) {
      continue;
    }
    // otherwise, we have not proven that the communicator is be different
    // TODO: (very advanced) if e.g. mpi comm split is used, we might be able to
    // statically prove different communicators
    // we could also insert a runtime check if communicators are different

    // check src/dest
    auto *src1 = src;
    auto *src2 = c->get_src();

    if (!(src1 == mpi_implementation_specifics->ANY_SOURCE ||
          src2 == mpi_implementation_specifics->ANY_SOURCE)) {
      // if it can match any, there is no reason to prove difference
      if (can_prove_val_different(src1, src2)) {
        continue;
      }
    }

    // check tag
    auto *tag1 = tag;
    auto *tag2 = c->get_tag();
    if (!(tag1 == mpi_implementation_specifics->ANY_TAG ||
          tag2 == mpi_implementation_specifics->ANY_TAG)) {
      // if it can match any, there is no reason to prove difference
      if (can_prove_val_different(tag1, tag2)) {
        continue;
      } // otherwise, we have not proven that the tag is different

      // could not disprove conflict
      perform_replacement = false;
      break;
    }
  }

  if (perform_replacement) {
    bool is_send = is_send_function(init_call->getCalledFunction());
    if (is_send) {
      assert(init_call->getCalledFunction() == mpi_func->mpi_send_init);
      replace_init_call(init_call, mpi_func->optimized.mpi_send_init_info,
                        get_runtime_check_result_true(init_call));
    } else {
      assert(init_call->getCalledFunction() == mpi_func->mpi_recv_init);
      replace_init_call(init_call, mpi_func->optimized.mpi_recv_init_info,
                        get_runtime_check_result_true(init_call));
    }
  }

  replaced = true;
}
