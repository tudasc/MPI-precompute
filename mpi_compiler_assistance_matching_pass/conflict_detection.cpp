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
#include "llvm/IR/Constants.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/CFG.h"

#include "debug.h"

using namespace llvm;

bool are_calls_conflicting(llvm::CallBase *orig_call,
                           llvm::CallBase *conflict_call, bool is_send);

bool uses_wildcard(CallBase *mpi_call, bool is_sending) {

  auto implementation_specifics = ImplementationSpecifics::get_instance();

  auto tag = get_tag(mpi_call, is_sending);
  auto src_rank = get_src(mpi_call, is_sending);

  return tag == implementation_specifics->ANY_TAG ||
         src_rank == implementation_specifics->ANY_SOURCE;
}

// True, if a conflicting call was found
bool check_call_for_conflict(CallBase *mpi_call, bool is_sending) {

  auto frontend_plugin_data = FrontendPluginData::get_instance();

  auto possible_conflicts =
      frontend_plugin_data->get_possibly_conflicting_calls(mpi_call);

  if (possible_conflicts.empty()) {
    return false; // frontend determined that no conflicts present
  }

  if (!is_sending && uses_wildcard(mpi_call, is_sending)) {
    return true; // wildcard always conflicts
  }

  for (auto other_call : possible_conflicts) {
    if (are_calls_conflicting(mpi_call, other_call, is_sending)) {
      return true;
    }
  }

  // no conflicts found
  return false;
}

bool check_mpi_send_conflicts(llvm::CallBase *send_init_call) {

  return check_call_for_conflict(send_init_call, true);
}

bool check_mpi_recv_conflicts(llvm::CallBase *recv_init_call) {
  return check_call_for_conflict(recv_init_call, false);
}

bool can_prove_val_different(Value *val_a, Value *val_b,
                             bool check_for_loop_iter_difference);

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
  /*Debug(if (result) {errs() << "Known different\n";} else {
   errs() << "could not prove difference\n";
   })*/

  return result;
}

// this function tries to prove if the given values differ for different loop
// iterations
bool can_prove_val_different_for_different_loop_iters(Value *val_a,
                                                      Value *val_b) {

  if (val_a != val_b) {
    return false;
  }

  assert((!isa<Constant>(val_a) || !isa<Constant>(val_b)) &&
         "This function should not be used with two constants");
  auto *inst_a = dyn_cast<Instruction>(val_a);
  auto *inst_b = dyn_cast<Instruction>(val_b);

  if (inst_b && !inst_a) {
    // we assume first param is an insruction (second may be constant)
    return can_prove_val_different_for_different_loop_iters(val_b, val_a);
  }

  assert(inst_a && "This should be an Instruction");
  assert(inst_a->getType()->isIntegerTy());

  LoopInfo *linfo = analysis_results->getLoopInfo(*inst_a->getFunction());
  ScalarEvolution *se = analysis_results->getSE(*inst_a->getFunction());
  assert(linfo != nullptr && se != nullptr);
  Loop *loop = linfo->getLoopFor(inst_a->getParent());

  if (loop) {
    auto *sc = se->getSCEV(inst_a);

    // if we can prove that the variable varies predictably with the loop, the
    // variable will be different for any two loop iterations otherwise the
    // variable is only LoopVariant but not predictable
    if (se->getLoopDisposition(sc, loop) ==
        ScalarEvolution::LoopDisposition::LoopComputable) {
      auto *sc_2 = se->getSCEV(val_b);
      // if vals are the same and predicable throuout the loop they differ each
      // iteration
      if (se->isKnownPredicate(CmpInst::Predicate::ICMP_EQ, sc, sc_2)) {
        assert(se->getLoopDisposition(sc, loop) !=
               ScalarEvolution::LoopDisposition::LoopInvariant);
        return true;

      } else {
        sc->print(errs());
        sc_2->print(errs());
      }
    }
  }

  return false;
}

// TODO this analysis may not work if a thread gets a pointer to another
// thread's stack, but whoever does that is dumb anyway...
bool can_prove_val_different(Value *val_a, Value *val_b) {

  // errs() << "Comparing: \n";
  // val_a->dump();
  // val_b->dump();

  if (val_a->getType() != val_b->getType()) {
    // this should not happen anyway
    assert(false && "Trying comparing values of different types.");
    return true;
  }

  if (auto *c1 = dyn_cast<Constant>(val_a)) {
    if (auto *c2 = dyn_cast<Constant>(val_b)) {
      if (c1 != c2) {
        // different constants
        // errs() << "Different\n";
        return true;
      } else {
        // proven same
        return false;
      }
    }
  }

  if (can_prove_val_different_with_scalarEvolution(val_a, val_b)) {
    return true;
  }

  /* if (check_for_loop_iter_difference) {
     if (can_prove_val_different_for_different_loop_iters(val_a, val_b)) {
       return true;
     }
   }*/

  // could not prove difference
  return false;
}

// true if there is a path from current pos containing block1 and then block2
// (and all blocks are in the loop) depth first search through the loop
bool is_path_in_loop_iter(BasicBlock *current_pos, bool encountered1,
                          Loop *loop, BasicBlock *block1, BasicBlock *block2,
                          std::set<BasicBlock *> &visited) {

  // end
  if (current_pos == block2) {
    return encountered1;
    // if we first discover block2 and then block 1 this does not count
  }

  bool has_encountered_1 = encountered1;
  if (current_pos == block1) {
    has_encountered_1 = true;
  }

  visited.insert(current_pos);

  auto *term = current_pos->getTerminator();

  for (unsigned int i = 0; i < term->getNumSuccessors(); ++i) {
    auto *next = term->getSuccessor(i);
    if (loop->contains(next) && visited.find(next) == visited.end()) {
      // do not leave one loop iter
      if (is_path_in_loop_iter(next, has_encountered_1, loop, block1, block2,
                               visited)) {
        // found path
        return true;
      }
    }
    // else search for other paths
  }

  // no path found
  return false;
}

// TODO does not work for all cases

// true if both calls are in the same loop and there is no loop iterations where
// both calls are called
bool are_calls_in_different_loop_iters(CallBase *orig_call,
                                       CallBase *conflict_call) {

  if (orig_call == conflict_call) {
    // call conflicting with itself may only be bart of one loop
    return true;
  }

  if (orig_call->getFunction() != conflict_call->getFunction()) {

    return false;
  }

  LoopInfo *linfo = analysis_results->getLoopInfo(*orig_call->getFunction());
  assert(linfo != nullptr);

  Loop *loop = linfo->getLoopFor(orig_call->getParent());

  if (!loop) { // not in loop
    return false;
  }

  if (loop != linfo->getLoopFor(conflict_call->getParent())) {
    // if in different loops or one is not in a loop
    return false;
  }

  assert(loop != nullptr &&
         loop == linfo->getLoopFor(conflict_call->getParent()));

  BasicBlock *orig_block = orig_call->getParent();
  BasicBlock *confilct_block = conflict_call->getParent();

  if (orig_block == confilct_block) {
    // obvious: true on every loop iteration
    return true;
  }

  // depth first search from the loop header to find a path through the
  // iteration containing both calls

  std::set<BasicBlock *> visited = {}; // start with empty set
  return !is_path_in_loop_iter(loop->getHeader(), false, loop, orig_block,
                               confilct_block, visited);
}

bool are_calls_conflicting(CallBase *orig_call, CallBase *conflict_call,
                           bool is_send) {

  // if one is send and the other a recv: fond a match which means no conflict
  /*
   if ((is_send && is_recv_function(conflict_call->getCalledFunction()))
   || (!is_send && is_send_function(conflict_call->getCalledFunction()))) {
   return false;
   }*/
  // was done before
  auto *mpi_implementation_specifics = ImplementationSpecifics::get_instance();

  // check communicator
  auto *comm1 = get_communicator(orig_call);
  auto *comm2 = get_communicator(conflict_call);
  if (can_prove_val_different(comm1, comm2)) {
    return false;
  }
  // otherwise, we have not proven that the communicator is be different
  // TODO: (very advanced) if e.g. mpi comm split is used, we might be able to
  // statically prove different communicators
  // we could also insert a runtime check if communicators are different

  // check src/dest
  auto *src1 = get_src(orig_call, is_send);
  auto *src2 = get_src(conflict_call, is_send);

  if (!(src1 == mpi_implementation_specifics->ANY_SOURCE ||
        src2 == mpi_implementation_specifics->ANY_SOURCE)) {
    // if it can match any, there is no reason to prove difference
    if (can_prove_val_different(src1, src2)) {
      return false;
    }
  }

  // check tag
  auto *tag1 = get_tag(orig_call, is_send);
  auto *tag2 = get_tag(conflict_call, is_send);
  if (!(tag1 == mpi_implementation_specifics->ANY_TAG ||
        tag2 == mpi_implementation_specifics->ANY_TAG)) {
    // if it can match any, there is no reason to prove difference
    if (can_prove_val_different(tag1, tag2)) {
      return false;
    } // otherwise, we have not proven that the tag is different
  }

  // cannot disprove conflict
  return true;
}

Value *get_communicator(CallBase *mpi_call) {

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

Value *get_src(CallBase *mpi_call, bool is_send) {

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

Value *get_tag(CallBase *mpi_call, bool is_send) {

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
