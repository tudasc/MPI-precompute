//
// Created by tim on 12.05.25.
//

#include "LoopOptimize.h"

#include "analysis_results.h"
#include "openmp_runtime_functions.h"

#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"
#include <cassert>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

bool check_if_tsan_licm_is_possible(
    Loop *loop, const std::vector<llvm::CallBase *> &tsan_in_loop) {
  auto SE = analysis_results->getSE(*loop->getHeader()->getParent());

  for (auto *call : tsan_in_loop) {
    // called func + ONE argument
    assert(call->getNumOperands() == 2);
    // call->dump();
    if (loop->isLoopInvariant(call->getArgOperand(0))) {
      // nothing to do, invariant anyway
      // llvm::errs() << "Loop invariant\n";
    } else {
      // check if bounds are know
      auto scev = SE->getSCEV(call->getArgOperand(0));
      if (not SE->hasComputableLoopEvolution(scev, loop)) {
        // need to execute
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

ConstantInt *get_size_of_tsan_access(CallBase *tsan_call) {
  // TODO implement
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

  tsan_call->dump();
  assert(false && "Size of tsan access not implemented");
  return nullptr;
}

// true if loop was optimized
bool perform_tsan_licm(llvm::Module &M, Loop *loop,
                       const std::vector<llvm::CallBase *> &tsan_in_loop) {
  if (not check_if_tsan_licm_is_possible(loop, tsan_in_loop)) {
    return false;
  }

  auto tsan_read_range_func = M.getOrInsertFunction(
      "__tsan_read_range", Type::getVoidTy(M.getContext()),
      Type::getInt8PtrTy(M.getContext()), Type::getInt64Ty(M.getContext()));
  auto tsan_write_range_func = M.getOrInsertFunction(
      "__tsan_write_range", Type::getVoidTy(M.getContext()),
      Type::getInt8PtrTy(M.getContext()), Type::getInt64Ty(M.getContext()));

  auto SE = analysis_results->getSE(*loop->getHeader()->getParent());
  BasicBlock *incoming;
  BasicBlock *outgoing;
  if (not loop->getIncomingAndBackEdge(incoming, outgoing)) {

    errs() << "Could not replace loop with one BB: incoming and backedge are "
              "not unique\n";
    return false;
  }
  outgoing = loop->getExitBlock();
  assert(incoming);
  assert(outgoing);
  // errs() << "create new BB instead of loop\n";
  BasicBlock *new_bb =
      BasicBlock::Create(loop->getHeader()->getContext(), "loop_replacement",
                         incoming->getParent(), outgoing);
  IRBuilder<> builder(new_bb);
  // dummy instruction serving as the insertion point to insert everything
  // before
  auto *dummy_inst =
      builder.CreateAlloca(builder.getInt64Ty(), nullptr, "dummy");

  for (auto *call : tsan_in_loop) {
    // call->dump();
    assert(call->getNumOperands() == 2);
    if (loop->isLoopInvariant(call->getArgOperand(0))) {
      builder.CreateCall(call->getCalledFunction(), call->getArgOperand(0));
    } else {
      auto scev = SE->getSCEV(call->getArgOperand(0));
      assert(SE->hasComputableLoopEvolution(scev, loop));

      // Assumes scev is AddRec for the loop
      assert(isa<SCEVAddRecExpr>(scev) && "Expected AddRec SCEV");
      auto *addRec = dyn_cast<SCEVAddRecExpr>(scev);

      auto tripCount = SE->getSymbolicMaxBackedgeTakenCount(loop);

      auto *start = addRec->getStart();
      auto *stop = addRec->evaluateAtIteration(tripCount, *SE);
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
      auto *as_ptr = builder.CreateIntToPtr(val_min, builder.getInt8PtrTy());
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
  // finish up BB
  builder.SetInsertPoint(dummy_inst);
  builder.CreateBr(outgoing);
  dummy_inst->eraseFromParent();
  // new_bb->dump();

  // set incoming BB

  BasicBlock *succ_to_replace = nullptr;
  auto *incoming_br = dyn_cast<BranchInst>(incoming->getTerminator());
  assert(incoming_br);
  int num_successors_replaced = 0;
  // find successor to replace and check if it is unique
  for (unsigned int i = 0; i < incoming_br->getNumSuccessors(); i++) {
    auto *succ = incoming_br->getSuccessor(i);
    // std::find
    bool in_loop = false;
    for (auto *bb : loop->getBlocks()) {
      if (succ == bb) {
        in_loop = true;
        break;
      }
    }
    if (in_loop) {
      incoming_br->setSuccessor(i, new_bb);
      num_successors_replaced++;
    }
  }
  assert(num_successors_replaced == 1);

  // remove old loop
  std::vector<BasicBlock *> to_delete;
  for (auto *bb : loop->getBlocks()) {
    bb->replaceAllUsesWith(new_bb); // if used in phi at outgoing
  }
  llvm::EliminateUnreachableBlocks(*new_bb->getParent());

  return true;
}

void Optimize_loops(llvm::Module &M) {
  unsigned int num_loops = 0;
  unsigned int optimized_loops = 0;
  // TODO collect loops first: then replace, iterating over the loops breaks
  for (auto it_f = M.begin(); it_f != M.end(); ++it_f) {
    Function *f = &*it_f;
    if (not f->isDeclaration()) {

      bool optimized = true;

      auto li = analysis_results->getLoopInfo(*f);
      while (optimized) { // ontil no more optimization
        optimized = false;
        // get new loop info

        for (auto loop : li->getLoopsInPreorder()) {
          num_loops++;

          bool loop_applicable = true;
          std::vector<llvm::CallBase *> tsan_calls;
          // collect tsan usage
          for (auto *bb : loop->getBlocks()) {
            for (auto it_i = bb->begin(); it_i != bb->end(); ++it_i) {
              llvm::Instruction *inst = &*it_i;
              if (auto *call = dyn_cast<CallBase>(inst)) {
                if (call->getCalledFunction() &&
                    // eiter tsan or omp function
                    // omp function necessary e.g. to keep synchronization
                    call->getCalledFunction()->getName().startswith("__tsan")) {
                  tsan_calls.push_back(call);
                } else if (is_omp_function(call->getCalledFunction())) {
                  // todo analyze if we may be able to do something here
                  loop_applicable = false;
                  break;
                } else {
                  // nothing we can do
                  loop_applicable = false;
                  break;
                }
              }
            }
          }

          if (loop_applicable) {
            if (perform_tsan_licm(M, loop, tsan_calls)) {
              optimized_loops++;
              optimized = true;
              return; // TODO handle properly!! probably use a domtree updater
              // LoopInfo is invalid
              li->erase(loop);
              break;
            }
          }
        }
      }
    }
  }
  // print statistics
  errs() << "Optimized loops: " << optimized_loops << "/" << num_loops << "\n";
}
