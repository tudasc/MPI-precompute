//
// Created by tim on 12.05.25.
//

#include "LoopOptimize.h"

#include "analysis_results.h"
#include "openmp_runtime_functions.h"
#include "std_funcs.h"

#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"
#include <cassert>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

ConstantInt *get_size_of_tsan_access(CallBase *tsan_call) {
  auto name = tsan_call->getCalledFunction()->getName();
  if (name == "__tsan_read1")
    return ConstantInt::get(Type::getInt64Ty(tsan_call->getContext()), 1);
  if (name == "__tsan_read4")
    return ConstantInt::get(Type::getInt64Ty(tsan_call->getContext()), 4);
  if (name == "__tsan_read8")
    return ConstantInt::get(Type::getInt64Ty(tsan_call->getContext()), 8);
  if (name == "__tsan_read16")
    return ConstantInt::get(Type::getInt64Ty(tsan_call->getContext()), 16);

  if (name == "__tsan_write1")
    return ConstantInt::get(Type::getInt64Ty(tsan_call->getContext()), 1);
  if (name == "__tsan_write4")
    return ConstantInt::get(Type::getInt64Ty(tsan_call->getContext()), 4);
  if (name == "__tsan_write8")
    return ConstantInt::get(Type::getInt64Ty(tsan_call->getContext()), 8);
  if (name == "__tsan_write16")
    return ConstantInt::get(Type::getInt64Ty(tsan_call->getContext()), 16);

  // tsan_call->dump();
  // assert(false && "Size of tsan access not implemented");
  return nullptr;
}

bool check_if_tsan_licm_is_possible(
    Loop *loop, const std::vector<llvm::CallBase *> &tsan_in_loop) {
  auto SE = analysis_results->getSE(*loop->getHeader()->getParent());

  for (auto *call : tsan_in_loop) {
    // called func + ONE argument
    if (not get_size_of_tsan_access(call)) {
      // TODO
      //  this tsan call is not supported yet
      errs() << "Loop Optimization fail: TSAN call not supported yet\n";
      call->dump();
      return false;
    }

    if (loop->isLoopInvariant(call->getArgOperand(0))) {
      // nothing to do, invariant anyway
      // llvm::errs() << "Loop invariant\n";
    } else {
      // check if bounds are know
      auto scev = SE->getSCEV(call->getArgOperand(0));
      if (not SE->hasComputableLoopEvolution(scev, loop)) {
        // need to execute
        errs() << "Loop Optimization fail: Ptr has non computable loop "
                  "Evolution\n";
        return false;
      }
      // else: we could compute the memory accesses before the loop
      // without running it and tell tsan that whole region is accessed
      // at once effectively
    }
  }

  auto trip_count = SE->getSymbolicMaxBackedgeTakenCount(loop);
  // loop bounds need to be computable as well
  return !isa<SCEVCouldNotCompute>(trip_count);
}

// if e.g. loop index is used after the loop
// todo not extensively tested!
bool compute_other_loop_values(llvm::Module &M, ScalarEvolution *SE, Loop *loop,
                               Instruction *insert_point) {
  const SCEV *exitCount = SE->getExitCount(loop, loop->getExitingBlock());
  if (isa<SCEVCouldNotCompute>(exitCount)) {
    errs() << "Loop Optimization fail: Could not compute loop exit count\n";
    return false;
  }

  std::map<Value *, Value *> replacement_map;
  for (auto *bb : loop->getBlocks()) {
    for (auto it_i = bb->begin(); it_i != bb->end(); ++it_i) {
      llvm::Instruction *inst_in_loop = &*it_i;
      for (auto *u : inst_in_loop->users()) {
        if (auto *user_inst = dyn_cast<Instruction>(u)) {
          if (not loop->contains(user_inst)) {
            if (not SE->isSCEVable(inst_in_loop->getType())) {
              return false;
            }
            auto scev = dyn_cast<SCEVAddRecExpr>(SE->getSCEV(inst_in_loop));
            if (not scev) {
              errs()
                  << "Could not compute Evolution of value used after loop\n";
              inst_in_loop->dump();
              return false;
            }
            auto *end_value_scev = scev->evaluateAtIteration(exitCount, *SE);
            SCEVExpander expander(*SE, M.getDataLayout(), "scev");
            expander.setInsertPoint(insert_point);

            Value *end_value =
                expander.expandCodeFor(end_value_scev, inst_in_loop->getType());
            replacement_map[inst_in_loop] = end_value;
            break; // only one replacement value is needed even if multiple
                   // users
          }
        }
      }
      // iterator through loop instr will stay intact, as we dont chenge the
      // loop
    }
  }
  // perform the replacement
  for (auto pair : replacement_map) {
    pair.first->replaceAllUsesWith(pair.second);
  }
  return true;
}

// removes the BB
void clean_temp_bb(BasicBlock *bb) {
  assert(bb->getNumUses() == 0);
  bb->eraseFromParent();
}

// true if loop was optimized
bool perform_tsan_licm(llvm::Module &M, Loop *loop,
                       const std::vector<llvm::CallBase *> &tsan_in_loop) {
  if (not check_if_tsan_licm_is_possible(loop, tsan_in_loop)) {
    return false;
  }

  auto tsan_read_range_func = M.getOrInsertFunction(
      "__tsan_read_range", Type::getVoidTy(M.getContext()),
      PointerType::get(M.getContext(), 0), Type::getInt64Ty(M.getContext()));
  auto tsan_write_range_func = M.getOrInsertFunction(
      "__tsan_write_range", Type::getVoidTy(M.getContext()),
      PointerType::get(M.getContext(), 0), Type::getInt64Ty(M.getContext()));

  auto SE = analysis_results->getSE(*loop->getHeader()->getParent());
  BasicBlock *incoming;
  BasicBlock *outgoing;
  if (not loop->getIncomingAndBackEdge(incoming, outgoing)) {

    errs() << "Loop Optimization Incoming and Back edge are not unique\n";
    return false;
  }
  outgoing = loop->getExitBlock();
  assert(incoming);
  if (!outgoing) {
    // TODO implement
    errs() << "Loop Optimization fail: Outgoing edge not unique\n";
    return false;
  }
  // errs() << "create new BB instead of loop\n";
  BasicBlock *new_bb =
      BasicBlock::Create(loop->getHeader()->getContext(), "loop_replacement",
                         incoming->getParent(), outgoing);
  IRBuilder<> builder(new_bb);
  // dummy instruction serving as the insertion point to insert everything
  // before
  auto *dummy_inst =
      builder.CreateAlloca(builder.getInt64Ty(), nullptr, "dummy");

  // check if other values, such as the loop index are used after the loop and
  // compute them if possible
  if (not compute_other_loop_values(M, SE, loop, dummy_inst)) {
    clean_temp_bb(new_bb);
    return false;
  }

  for (auto *call : tsan_in_loop) {
    // call->dump();
    assert(call->getNumOperands() == 2);
    if (loop->isLoopInvariant(call->getArgOperand(0))) {
      builder.SetInsertPoint(dummy_inst);
      builder.CreateCall(call->getCalledFunction(), call->getArgOperand(0));
    } else {
      auto scev = SE->getSCEV(call->getArgOperand(0));
      assert(SE->hasComputableLoopEvolution(scev, loop));

      auto *addRec = dyn_cast<SCEVAddRecExpr>(scev);
      if (!addRec) {
        // could not determine start and end value
        clean_temp_bb(new_bb);
        errs() << "Loop Optimization fail: Could not compute start and end "
                  "values of ptr\n";
        return false;
      }

      auto tripCount = SE->getSymbolicMaxBackedgeTakenCount(loop);

      auto *start = addRec->getStart();
      auto *stop = addRec->evaluateAtIteration(tripCount, *SE);

      if (!SE->isKnownPredicate(ICmpInst::ICMP_ULE, start, stop)) {
        std::swap(start, stop); // "backward" loop
        if (!SE->isKnownPredicate(ICmpInst::ICMP_ULE, start, stop)) {
          // could not determine iteration order
          clean_temp_bb(new_bb);
          errs() << "Loop Optimization fail: Could not determine iteration "
                    "order\n";
          return false;
        }
      }

      // start->dump();
      // stop->dump();

      // Expand to runtime values
      // Expand SCEV at min/max trip count
      SCEVExpander expander(*SE, M.getDataLayout(), "scev");
      expander.setInsertPoint(dummy_inst);

      Value *val_min = expander.expandCodeFor(start, builder.getInt64Ty());
      Value *val_max = expander.expandCodeFor(stop, builder.getInt64Ty());

      // create tsan call
      builder.SetInsertPoint(dummy_inst);
      auto *as_ptr = builder.CreateIntToPtr(val_min, builder.getPtrTy());
      auto *size = builder.CreateSub(val_max, val_min);
      // need to include the size of last access
      auto *size_full = builder.CreateAdd(size, get_size_of_tsan_access(call));

      if (call->getCalledFunction()->getName().starts_with("__tsan_read")) {
        // not supported right now
        assert(not call->getCalledFunction()->getName().starts_with(
            "__tsan_read_write"));
        builder.CreateCall(tsan_read_range_func, {as_ptr, size_full});
      } else {
        assert(
            call->getCalledFunction()->getName().starts_with("__tsan_write"));
        builder.CreateCall(tsan_write_range_func, {as_ptr, size_full});
      }
    }
  }

  // finish up replacement BB
  builder.SetInsertPoint(dummy_inst);
  builder.CreateBr(outgoing);
  dummy_inst->eraseFromParent();
  /*
    errs() << "Loop replaced:\n";
    for (auto bb : loop->getBlocks()) {
      bb->dump();
    }
    errs() << "replaced with:\n";
    new_bb->dump();
  */
  // set incoming BB
  auto *incoming_br = dyn_cast<BranchInst>(incoming->getTerminator());
  assert(incoming_br);
  int num_successors_replaced = 0;
  // find successor to replace and check if it is unique
  for (unsigned int i = 0; i < incoming_br->getNumSuccessors(); i++) {
    auto *succ = incoming_br->getSuccessor(i);
    if (loop->contains(succ)) {
      incoming_br->setSuccessor(i, new_bb);
      num_successors_replaced++;
    }
  }
  assert(num_successors_replaced == 1);

  // remove old loop
  std::vector<BasicBlock *> to_delete;
  for (auto *bb : loop->getBlocks()) {
    bb->replaceAllUsesWith(new_bb);
    to_delete.push_back(bb);
  }
  for (auto *bb : to_delete) {
    // dont care about correct deletion order, we already checked that nothing
    // more is used outside of loop
    for (auto it_i = bb->begin(); it_i != bb->end(); ++it_i) {
      Instruction *inst = &*it_i;
      inst->replaceAllUsesWith(PoisonValue::get(inst->getType()));
    }
    bb->eraseFromParent();
  }

  return true;
}

void Optimize_loops(llvm::Module &M) {
  unsigned int optimized_loops = 0;
  for (auto it_f = M.begin(); it_f != M.end(); ++it_f) {
    Function *f = &*it_f;
    if (not f->isDeclaration() && not is_func_from_std((f))) {

      bool optimized = true;
      while (optimized) { // until no more optimization
        optimized = false;
        // get new loop info if it was invalidated
        auto li = analysis_results->getLoopInfo(*f);

        for (auto loop : li->getLoopsInPreorder()) {

          bool loop_applicable = true;
          std::vector<llvm::CallBase *> tsan_calls;
          // collect tsan usage
          for (auto *bb : loop->getBlocks()) {
            for (auto it_i = bb->begin(); it_i != bb->end(); ++it_i) {
              llvm::Instruction *inst = &*it_i;
              if (auto *call = dyn_cast<CallBase>(inst)) {
                if (call->getCalledFunction() &&
                    call->getCalledFunction()->getName().starts_with(
                        "__tsan")) {
                  tsan_calls.push_back(call);
                } else if (call->getCalledFunction() &&
                           is_thread_function(call->getCalledFunction())) {
                  // todo analyze if we may be able to do something here?
                  errs() << "Loop Optimization fail: Call to Openmp\n";
                  call->dump();
                  loop_applicable = false;
                  break;
                } else {
                  // call to something else: we cant analyze that

                  loop_applicable = false;
                  errs() << "Loop Optimization fail: Call in loop\n";
                  call->dump();
                  break;
                }
              }
              if (auto *store = dyn_cast<StoreInst>(inst)) {
                // some computation result may be necessary
                loop_applicable = false;
                errs() << "Loop Optimization fail: store in loop\n";
                inst->dump();
                // debug info
                auto SE =
                    analysis_results->getSE(*loop->getHeader()->getParent());
                auto scev = SE->getSCEV(store->getPointerOperand()); // ptr
                errs() << "Ptr: Invariant? " << SE->isLoopInvariant(scev, loop)
                       << " Konwn Evolution? "
                       << SE->hasComputableLoopEvolution(scev, loop) << "\n";

                break;
              }
            }
          }

          if (loop_applicable) {
            if (perform_tsan_licm(M, loop, tsan_calls)) {
              optimized_loops++;
              optimized = true;
              // LoopInfo is invalid!
              analysis_results->invalidate(*f);
              break; // end looping over loops, as iterator is invalid
            }
          }
        }
      }
    }
  }
  // print statistics
  errs() << "Optimized loops: " << optimized_loops << "\n";
}
