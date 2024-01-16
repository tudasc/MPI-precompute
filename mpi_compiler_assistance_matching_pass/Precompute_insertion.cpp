// TODO clean includes
#include "CompilerPassConstants.h"
#include "conflict_detection.h"
#include "implementation_specific.h"
#include "mpi_functions.h"
#include "precalculation.h"
#include "precompute_funcs.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"

#include "Precompute_insertion.h"

#include "debug.h"

#include "VtableManager.h"

using namespace llvm;

void PrecalculationFunctionCopy::initialize_copy() {
  assert(F_copy == nullptr);
  assert(not F_orig->isDeclaration() && "Cannot copy external function");
  F_copy = CloneFunction(F_orig, old_new_map, cloned_code_info);

  for (auto v : old_new_map) {
    auto *old_v = const_cast<Value *>(v.first);
    Value *new_v = v.second;
    new_to_old_map.insert(std::make_pair(new_v, old_v));
  }
}

llvm::Function *get_global_re_init_function(
    Module &M, const PrecalculationAnalysis &precompute_analyis_result) {
  auto *implementation_specifics = ImplementationSpecifics::get_instance();

  auto *func = Function::Create(
      FunctionType::get(Type::getVoidTy(M.getContext()), false),
      llvm::GlobalValue::InternalLinkage, "re_init_globals", M);
  auto *bb = BasicBlock::Create(M.getContext(), "", func, nullptr);
  assert(bb->isEntryBlock());
  IRBuilder<> builder(bb);

  for (auto &global : M.globals()) {
    // if Comm World is needed there is no need to initialize it again, it
    // cannot be modified comm World will have no ptr info and therefore is
    // Written to check cannot be made
    if (precompute_analyis_result.is_included_in_precompute(&global) &&
        &global != implementation_specifics->COMM_WORLD) {
      assert(global.getType()->isPointerTy());
      auto global_info = precompute_analyis_result.get_taint_info(&global);

      assert(global_info->ptr_info);
      if (global_info->ptr_info->isWrittenTo()) {
        if (global.hasInitializer()) {
          builder.CreateStore(global.getInitializer(), &global);
        } else {
          // TODO @_ZSt4cout = external global %"class.std::basic_ostream",
          // align 8
          //  can we modify this global to point towards /dev/null?
          //  this would "resolve" the issue that writing to stdout can have an
          //  exception
          // and the user would see the output just to check if an exception is
          // raised
          errs() << "Global without initializer:\n";
          global.dump();
          assert(is_global_from_std(&global));
        }
      } // else no need to do anything as it is not changed (readonly)
      // at least not by tainted instructions
    }
  }

  builder.CreateRetVoid();

  return func;
}

void replace_allocation_call(llvm::CallBase *call) {
  assert(call);
  assert(is_allocation(call));

  Value *size;
  IRBuilder<> builder = IRBuilder<>(call);

  if (call->arg_size() == 1) {
    size = call->getArgOperand(0);
  } else {
    // calloc has num elements and size of elements
    assert(call->arg_size() == 2);
    assert(call->getCalledFunction()->getName() == "calloc");
    // TODO if both are constant, we should do constant propergation
    size = builder.CreateMul(call->getArgOperand(0), call->getArgOperand(1));
  }
  assert(size);
  CallBase *new_call;

  if (isa<CallInst>(call)) {
    new_call =
        builder.CreateCall(PrecomputeFunctions::get_instance()->allocate_memory,
                           {size}, call->getName());
  } else {
    assert(isa<InvokeInst>(call));
    auto *ivoke = cast<InvokeInst>(call);
    new_call = builder.CreateInvoke(
        PrecomputeFunctions::get_instance()->allocate_memory,
        ivoke->getNormalDest(), ivoke->getUnwindDest(), {size},
        call->getName());
  }

  call->replaceAllUsesWith(new_call);
  call->eraseFromParent();
}

// nullptr otherwise
std::shared_ptr<PrecalculationFunctionCopy> is_in_a_precompute_copy_func(
    llvm::Instruction *inst,
    const std::map<llvm::Function *,
                   std::shared_ptr<PrecalculationFunctionCopy>>
        &functions_copied) {

  auto *func = inst->getFunction();
  auto pos = std::find_if(
      functions_copied.begin(), functions_copied.end(),
      [func](const auto &pair) { return pair.second->F_copy == func; });
  if (pos != functions_copied.end()) {
    return pos->second;
  } else {
    return nullptr;
  }
}

void replace_usages_of_func_in_copy(
    const std::shared_ptr<PrecalculationFunctionCopy> &func,
    const std::map<llvm::Function *,
                   std::shared_ptr<PrecalculationFunctionCopy>>
        &functions_copied) {
  std::vector<Instruction *> instructions_to_change;
  for (auto *u : func->F_orig->users()) {
    if (auto *inst = dyn_cast<Instruction>(u)) {
      if (is_in_a_precompute_copy_func(inst, functions_copied)) {
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
    if (isa<GlobalAlias>(u)) {
      // alias -- can be ignored as analysis will make sure that no-one uses the
      // alias
      assert(func->analysis_result->getAliases().find(cast<GlobalAlias>(u)) !=
             func->analysis_result->getAliases().end());
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

CallBase *replace_MPI_with_precompute(
    const std::shared_ptr<PrecalculationFunctionCopy> &func, CallBase *call) {
  auto *tag = get_tag_value(call, true);
  auto *src = get_src_value(call, true);
  IRBuilder<> builder = IRBuilder<>(call);

  int precompute_envelope_dest;
  int precompute_envelope_tag;
  if (call->getCalledFunction() == mpi_func->mpi_send_init) {
    precompute_envelope_dest = SEND_ENVELOPE_DEST;
    precompute_envelope_tag = SEND_ENVELOPE_TAG;
  } else {
    assert(call->getCalledFunction() == mpi_func->mpi_recv_init);
    precompute_envelope_dest = RECV_ENVELOPE_DEST;
    precompute_envelope_tag = RECV_ENVELOPE_TAG;
  }

  builder.CreateCall(
      PrecomputeFunctions::get_instance()->register_precomputed_value,
      {builder.getInt32(precompute_envelope_dest), src});

  auto *new_call = builder.CreateCall(
      PrecomputeFunctions::get_instance()->register_precomputed_value,
      {builder.getInt32(precompute_envelope_tag), tag});

  Instruction *invoke_br = nullptr;
  if (auto *invoke = dyn_cast<InvokeInst>(call)) {
    // the register precompute call does not throw exceptions so we don't
    // need an invoke
    invoke_br = builder.CreateBr(invoke->getNormalDest());
  }
  call->replaceAllUsesWith(ImplementationSpecifics::get_instance()->SUCCESS);
  auto *old_call_v = func->new_to_old_map[call];
  call->eraseFromParent();

  func->new_to_old_map[new_call] = old_call_v;
  if (invoke_br) {
    func->new_to_old_map[invoke_br] = old_call_v;
  }
  return call;
}

// sometimes different member funcs of objects are relevant
//  example: Base: foo, bar
//  for inherited1: foo is relevant, for inherited2 bar is relevant
//  in this case vtable for inherited1 foo will be null as it is not needed
//  therefore we need to check against null and skip the call if it is not
//  necessary for the given instance
void surround_indirect_call_with_nullptr_check(
    const std::shared_ptr<PrecalculationFunctionCopy> &func, CallBase *call) {
  assert(call->isIndirectCall());

  auto *BB = call->getParent();
  BasicBlock *continueBB;
  if (auto *invoke = dyn_cast<InvokeInst>(call)) {
    continueBB = invoke->getNormalDest();
  } else {
    continueBB = BB->splitBasicBlock(call->getNextNode());
    // map all new instructions created to the old call
    func->new_to_old_map[BB->getTerminator()] = func->new_to_old_map[call];
  }

  auto *callBB = BB->splitBasicBlock(call);

  auto *unconditionalBR = BB->getTerminator();
  // the unconditional br created by splitBasicBlock
  IRBuilder<> builder(unconditionalBR);
  auto *null_p = ConstantPointerNull::get(
      cast<PointerType>(call->getCalledOperand()->getType()));
  auto *cmp = builder.CreateCmp(CmpInst::Predicate::ICMP_NE,
                                call->getCalledOperand(), null_p);
  auto *condbr = builder.CreateCondBr(cmp, callBB, continueBB);

  // map all new instructions created to the old call
  func->new_to_old_map[cmp] = func->new_to_old_map[call];
  func->new_to_old_map[condbr] = func->new_to_old_map[call];

  unconditionalBR->eraseFromParent();
}

void replace_calls_in_copy(
    const std::shared_ptr<PrecalculationFunctionCopy> &func,
    const PrecalculationAnalysis &precompute_analyis_result,
    const std::map<llvm::Function *,
                   std::shared_ptr<PrecalculationFunctionCopy>>
        &functions_copied) {
  std::vector<CallBase *> to_replace;

  // first  gather calls that need replacement so that the iterator does not
  // get broken if we remove stuff

  for (auto I = inst_begin(func->F_copy), E = inst_end(func->F_copy); I != E;
       ++I) {
    if (auto *call = dyn_cast<CallBase>(&*I)) {
      if (call->isIndirectCall()) {
        to_replace.push_back(call);
      } else {
        auto *callee = call->getCalledFunction();

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

        if (is_allocation(call)) {
          to_replace.push_back(call);
          continue;
        }

        if (isa<InvokeInst>(call)) {
          to_replace.push_back(call);
          continue;
        }

        if (precompute_analyis_result.is_func_included_in_precompute(
                call->getCalledFunction())) {
          to_replace.push_back(call);
          continue;
        } else {
          // call->dump();
          assert(
              not precompute_analyis_result.is_included_in_precompute(callee) ||
              // callee is the original function
              // which should not be a user function
              is_func_from_std(callee) || is_mpi_function(callee) ||
              callee->isIntrinsic());
          // it is not used: nothing to do, later pruning step will remove it
        }
      }
    }
  }

  for (auto *call : to_replace) {
    Value *orig_call_v = func->new_to_old_map[call];
    auto *orig_call = dyn_cast<CallBase>(orig_call_v);
    assert(orig_call);

    auto *callee = call->getCalledFunction();
    if (callee == mpi_func->mpi_send_init) {
      call = replace_MPI_with_precompute(func, call);
      continue;
    }
    if (callee == mpi_func->mpi_recv_init) {
      call = replace_MPI_with_precompute(func, call);

      continue;
    }
    // end handling calls to MPI

    if (is_allocation(call)) {
      replace_allocation_call(call);
      continue;
    }

    if (auto *invoke = dyn_cast<InvokeInst>(call)) {
      // special case: it is an invoke that we don't need to call because no
      // meaningful exception can be raised

      errs() << "Test Removal:\n";
      call->dump();

      bool can_omit = true;
      for (auto *target :
           precompute_analyis_result.get_possible_call_targets(orig_call)) {

        errs() << target->getName() << "\n";

        auto func_info =
            precompute_analyis_result.get_function_analysis(target);
        if (func_info->include_in_precompute ||
            precompute_analyis_result.is_retval_of_call_needed(orig_call) ||
            precompute_analyis_result.is_invoke_exception_case_needed(
                cast<InvokeInst>(orig_call))) {
          errs() << "Can NOT omit: needed for except case?"
                 << precompute_analyis_result.is_invoke_exception_case_needed(
                        cast<InvokeInst>(orig_call))
                 << " needed for callee?" << func_info->include_in_precompute
                 << "\n"
                 << "included? "
                 << precompute_analyis_result.is_included_in_precompute(
                        orig_call)
                 << "\n";
          can_omit = false;
        }
      }

      if (can_omit) {

        errs() << "REMOVE:  ";
        invoke->dump();
        IRBuilder<> builder = IRBuilder<>(invoke);
        auto br = builder.CreateBr(invoke->getNormalDest());
        func->new_to_old_map[br] = func->new_to_old_map[invoke];
        invoke->replaceAllUsesWith(UndefValue::get(invoke->getType()));
        // all users of retval will be removed in prune_function_copy
        invoke->eraseFromParent();
        continue;
      }
    }

    try {
      auto function_information =
          functions_copied.at(call->getCalledFunction());

      if (not call->isIndirectCall()) {
        call->setCalledFunction(function_information->F_copy);
        // null all not used args
      }

      for (unsigned int i = 0; i < call->arg_size(); ++i) {
        if (not precompute_analyis_result.is_included_in_precompute(
                orig_call->getArgOperand(i))) {
          // set unused arg to 0 (so we don't need to compute it)
          call->setArgOperand(
              i, Constant::getNullValue(call->getArgOperand(i)->getType()));
        } // else pass arg normally
      }
    } catch (std::out_of_range &e) {
      if (not call->isIndirectCall()) {
        // callee is the original function
        // which should not be a user function
        // call->dump();
        assert(is_func_from_std(callee) || is_mpi_function(callee) ||
               callee->isIntrinsic());
      }
    }

    if (call->isIndirectCall()) {
      if (not precompute_analyis_result.is_retval_of_call_needed(orig_call)) {
        surround_indirect_call_with_nullptr_check(func, call);
        // if retval is used: the precompute vtable can not contain null
        // TODO performance: only insert null check if we know the precompute
        // vtable can contain null
      }
    }
  }

  replace_usages_of_func_in_copy(func, functions_copied);
}

// remove all unnecessary instruction
void prune_function_copy(
    const std::shared_ptr<PrecalculationFunctionCopy> &func,
    const PrecalculationAnalysis &precompute_analyis_result) {
  std::vector<Instruction *> to_prune;

  // first  gather all instructions, so that the iterator does not get broken
  // if we remove stuff
  for (auto I = inst_begin(func->F_copy), E = inst_end(func->F_copy); I != E;
       ++I) {

    Instruction *inst = &*I;
    auto *old_v = func->new_to_old_map[inst];
    if (not precompute_analyis_result.is_included_in_precompute(old_v)) {
      if (auto *call = dyn_cast<CallBase>(inst)) {
        if (PrecomputeFunctions::get_instance()->is_call_to_precompute(call)) {
          // do not remove

        } else {
          to_prune.push_back(inst);
        }

      } else {
        to_prune.push_back(inst);
      }
    } else if (isa<InvokeInst>(inst)) {
      // an invoke can be tainted only because it may return an exception
      // but it actually is exception free for our purpose
      // meaning if it throws no MPI is used

      assert(precompute_analyis_result.is_included_in_precompute(old_v));
      auto *old_ivoke = dyn_cast<InvokeInst>(old_v);
      assert(old_ivoke);

      if (not precompute_analyis_result.is_invoke_necessary_for_control_flow(
              old_ivoke) &&
          not precompute_analyis_result.is_retval_of_call_needed(old_ivoke)) {
        if (not(is_func_from_std(old_ivoke->getCalledFunction()) ||
                is_mpi_call(old_ivoke))) {
          to_prune.push_back(inst);
        } else {
          // call to std or mpi

          // call to std or MPI will be kept if all params are tainted
          bool all_tainted = true;

          for (auto &arg : old_ivoke->args()) {
            if (not precompute_analyis_result.is_included_in_precompute(
                    cast<Value>(&arg))) {
              all_tainted = false;
            }
          }

          if (not precompute_analyis_result.can_except_in_precompute(
                  old_ivoke) &&
              not all_tainted) {
            // can be replaced with unconditional br to normal dest
            to_prune.push_back(inst);
          }
        }
      }
    }
  }

  // remove stuff
  for (auto *inst : to_prune) {
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
    if (auto *lp = dyn_cast<LandingPadInst>(inst)) {
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

void add_call_to_precalculation_to_main(
    llvm::Module &M,
    const std::shared_ptr<PrecalculationFunctionCopy> &entry_function,
    const PrecalculationAnalysis &precompute_analyis_result) {
  // TODO code duplication wir auto pos=

  // search for MPI_init or Init Thread as precalc may only take place after
  // that
  CallBase *call_to_init = nullptr;
  if (mpi_func->mpi_init != nullptr) {
    for (auto *u : mpi_func->mpi_init->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        assert(call_to_init == nullptr && "MPI_Init is only allowed once");
        call_to_init = call;
      }
    }
  }
  if (mpi_func->mpi_init_thread != nullptr) {
    for (auto *u : mpi_func->mpi_init_thread->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        assert(call_to_init == nullptr && "MPI_Init is only allowed once");
        call_to_init = call;
      }
    }
  }

  assert(call_to_init != nullptr && "Did Not Found MPI_Init_Call");

  assert(call_to_init->getFunction() == entry_function->F_orig &&
         "MPI_Init is not in main");

  // insert after init
  // MPIOPT_Init will later be inserted between this 2 calls
  IRBuilder<> builder(call_to_init->getNextNode());

  auto *precompute_funcs = PrecomputeFunctions::get_instance();

  // forward args of main
  std::vector<Value *> args;
  for (auto &arg : entry_function->F_orig->args()) {
    args.push_back(&arg);
  }
  builder.CreateCall(precompute_funcs->init_precompute_lib);
  builder.CreateCall(entry_function->F_copy, args);
  builder.CreateCall(precompute_funcs->finish_precomputation);
  auto *re_init_fun = get_global_re_init_function(M, precompute_analyis_result);
  builder.CreateCall(re_init_fun);
}

void insert_precomputation(
    llvm::Module &M, const PrecalculationAnalysis &precompute_analyis_result) {

  std::map<llvm::Function *, std::shared_ptr<PrecalculationFunctionCopy>>
      functions_copied;
  auto vtm = VtableManager(M);
  for (const auto &f : precompute_analyis_result.getFunctionsToInclude()) {
    auto f_copy = std::make_shared<PrecalculationFunctionCopy>(f);
    assert(f->func == f_copy->F_orig);
    functions_copied[f->func] = f_copy;
    vtm.register_function_copy(f->func, f_copy->F_copy);
  }

  vtm.perform_vtable_change_in_copies();
  // don't fuse this loops! we first need to initialize the copy before changing
  // calls
  for (const auto &pair : functions_copied) {
    replace_calls_in_copy(pair.second, precompute_analyis_result,
                          functions_copied);
  }

  for (const auto &pair : functions_copied) {
    prune_function_copy(pair.second, precompute_analyis_result);
  }

  auto *entry_point = precompute_analyis_result.getEntryPoint();
  assert(entry_point);
  auto entry_point_copy = functions_copied[entry_point];
  if (entry_point_copy) {
    // otherwise: nothing to do nothing to precalculate was found
    add_call_to_precalculation_to_main(M, entry_point_copy,
                                       precompute_analyis_result);
  }
}
