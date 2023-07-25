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

#include "replacement.h"
#include "analysis_results.h"
#include "conflict_detection.h"

#include "implementation_specific.h"
#include "mpi_functions.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "debug.h"
#include "nc_settings.h"

using namespace llvm;

bool add_init(llvm::Module &M) {

  bool result = false;
  if (mpi_func->mpi_init) {
    assert(mpi_func->optimized.init);
    for (auto *u : mpi_func->mpi_init->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {

        auto *insert_pt = call->getNextNode();
        if (auto *invoke = dyn_cast<InvokeInst>(call)) {
          insert_pt = invoke->getNormalDest()->getFirstNonPHI();
        }

        IRBuilder<> builder(insert_pt);
        builder.CreateCall(mpi_func->optimized.init);
        result = true;
      }
    }
  }

  if (mpi_func->mpi_init_thread) {
    assert(mpi_func->optimized.init);
    for (auto *u : mpi_func->mpi_init_thread->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {

        auto *insert_pt = call->getNextNode();
        if (auto *invoke = dyn_cast<InvokeInst>(call)) {
          insert_pt = invoke->getNormalDest()->getFirstNonPHI();
        }

        IRBuilder<> builder(insert_pt);
        builder.CreateCall(mpi_func->optimized.init);
        result = true;
      }
    }
  }
  return result;
}

bool add_finalize(llvm::Module &M) {
  bool result = false;
  if (mpi_func->mpi_finalize) {
    assert(mpi_func->optimized.finalize);
    for (auto *u : mpi_func->mpi_finalize->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        IRBuilder<> builder(call);
        builder.CreateCall(mpi_func->optimized.finalize);
        result = true;
      }
    }
  }
  return result;
}

void replace_call(CallBase *call, Function *func) {

  assert(call->getFunctionType() == func->getFunctionType());

  IRBuilder<> builder(call->getContext());

  std::vector<Value *> args;

  for (unsigned int i = 0; i < call->arg_size(); ++i) {
    args.push_back(call->getArgOperand(i));
  }

  auto *new_call = builder.CreateCall(func->getFunctionType(), func, args);

  ReplaceInstWithInst(call, new_call);

  // call->replaceAllUsesWith(new_call);
  // call->eraseFromParent();
}

void replace_init_call(llvm::CallBase *call, llvm::Function *func,
                       llvm::Value *runtime_check_result) {

  // one could assert that the only addition is the info object
  assert(call->getFunctionType() != func->getFunctionType());

  IRBuilder<> builder(call->getContext());

  // before the original call
  builder.SetInsertPoint(call);
  auto *mpi_implementation_specifics = ImplementationSpecifics::get_instance();

  auto info_obj_ptr =
      builder.CreateAlloca(mpi_implementation_specifics->mpi_info);

  builder.CreateCall(mpi_func->mpi_info_create->getFunctionType(),
                     mpi_func->mpi_info_create, {info_obj_ptr});

  auto info_obj =
      builder.CreateLoad(mpi_implementation_specifics->mpi_info, info_obj_ptr);
  std::vector<Value *> args;

  // set a key value pair to the info object:
  auto key = builder.CreateGlobalStringPtr("nc_send_strategy");
  auto value = builder.CreateGlobalStringPtr(STRATEGY);
  builder.CreateCall(mpi_func->mpi_info_set->getFunctionType(),
                     mpi_func->mpi_info_set, {info_obj, key, value});

  key = builder.CreateGlobalStringPtr("nc_mixed_threshold");
  char threshold_str[30];
  sprintf(threshold_str, "%d", THRESHOLD);
  value = builder.CreateGlobalStringPtr(threshold_str);
  builder.CreateCall(mpi_func->mpi_info_set->getFunctionType(),
                     mpi_func->mpi_info_set, {info_obj, key, value});

  // enable skipping of matching
  key = builder.CreateGlobalStringPtr("skip_matching");
  builder.CreateCall(mpi_func->mpi_info_set->getFunctionType(),
                     mpi_func->mpi_info_set,
                     {info_obj, key, runtime_check_result});

  for (unsigned int i = 0; i < call->arg_size(); ++i) {
    args.push_back(call->getArgOperand(i));
  }
  args.push_back(info_obj);

  auto new_call = builder.CreateCall(func->getFunctionType(), func, args);

  // also free the info
  builder.CreateCall(mpi_func->mpi_info_free->getFunctionType(),
                     mpi_func->mpi_info_free, info_obj_ptr);

  call->replaceAllUsesWith(new_call);
  call->eraseFromParent();
}

std::vector<CallBase *> get_request_handling_calls_for(Module &M, Function *f) {
  std::vector<CallBase *> result;
  if (f) {
    for (auto *u : f->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        if (call->getCalledFunction() == f) {
          result.push_back(call);
        }
      }
    }
  }
  return result;
}

// calls like start wait test
std::vector<CallBase *> get_request_handling_calls(Module &M) {

  std::vector<CallBase *> calls_to_replace;

  auto temp = get_request_handling_calls_for(M, mpi_func->mpi_start);
  calls_to_replace.insert(calls_to_replace.end(), temp.begin(), temp.end());
  temp = get_request_handling_calls_for(M, mpi_func->mpi_startall);
  calls_to_replace.insert(calls_to_replace.end(), temp.begin(), temp.end());

  temp = get_request_handling_calls_for(M, mpi_func->mpi_wait);
  calls_to_replace.insert(calls_to_replace.end(), temp.begin(), temp.end());
  temp = get_request_handling_calls_for(M, mpi_func->mpi_waitall);
  calls_to_replace.insert(calls_to_replace.end(), temp.begin(), temp.end());
  temp = get_request_handling_calls_for(M, mpi_func->mpi_waitany);
  calls_to_replace.insert(calls_to_replace.end(), temp.begin(), temp.end());
  temp = get_request_handling_calls_for(M, mpi_func->mpi_waitsome);
  calls_to_replace.insert(calls_to_replace.end(), temp.begin(), temp.end());

  temp = get_request_handling_calls_for(M, mpi_func->mpi_test);
  calls_to_replace.insert(calls_to_replace.end(), temp.begin(), temp.end());
  temp = get_request_handling_calls_for(M, mpi_func->mpi_testall);
  calls_to_replace.insert(calls_to_replace.end(), temp.begin(), temp.end());
  temp = get_request_handling_calls_for(M, mpi_func->mpi_testany);
  calls_to_replace.insert(calls_to_replace.end(), temp.begin(), temp.end());
  temp = get_request_handling_calls_for(M, mpi_func->mpi_testsome);
  calls_to_replace.insert(calls_to_replace.end(), temp.begin(), temp.end());

  temp = get_request_handling_calls_for(M, mpi_func->mpi_request_free);
  calls_to_replace.insert(calls_to_replace.end(), temp.begin(), temp.end());

  return calls_to_replace;
}

void replace_request_handling_calls(llvm::Module &M) {

  auto calls_to_replace = get_request_handling_calls(M);

  // do the actual replacement
  for (auto *call : calls_to_replace) {
    if (call->getCalledFunction() == mpi_func->mpi_wait) {
      replace_call(call, mpi_func->optimized.mpi_wait);
    } else if (call->getCalledFunction() == mpi_func->mpi_waitall) {
      replace_call(call, mpi_func->optimized.mpi_waitall);
    } else if (call->getCalledFunction() == mpi_func->mpi_waitany) {
      replace_call(call, mpi_func->optimized.mpi_waitany);
    } else if (call->getCalledFunction() == mpi_func->mpi_waitsome) {
      replace_call(call, mpi_func->optimized.mpi_waitsome);

    } else if (call->getCalledFunction() == mpi_func->mpi_test) {
      replace_call(call, mpi_func->optimized.mpi_test);
    } else if (call->getCalledFunction() == mpi_func->mpi_testall) {
      replace_call(call, mpi_func->optimized.mpi_testall);
    } else if (call->getCalledFunction() == mpi_func->mpi_testany) {
      replace_call(call, mpi_func->optimized.mpi_testany);
    } else if (call->getCalledFunction() == mpi_func->mpi_testsome) {
      replace_call(call, mpi_func->optimized.mpi_testsome);

    } else if (call->getCalledFunction() == mpi_func->mpi_start) {
      replace_call(call, mpi_func->optimized.mpi_start);
    } else if (call->getCalledFunction() == mpi_func->mpi_startall) {
      replace_call(call, mpi_func->optimized.mpi_startall);

    } else if (call->getCalledFunction() == mpi_func->mpi_request_free) {
      replace_call(call, mpi_func->optimized.mpi_request_free);

    } else {

      errs() << "This MPI call is currently NOT supported\n";
      call->dump();

      assert(false);
    }
  }
}

// returns the llvm value that represents is the result of the runtime check
llvm::Value *insert_runtime_check(llvm::Value *val_a, llvm::Value *val_b) {
  assert(false && "Not implemented");
}

// returns the llvm value that represents is the result of the runtime check
llvm::Value *combine_runtime_checks(llvm::CallBase *call,
                                    llvm::Value *result_src,
                                    llvm::Value *result_tag,
                                    llvm::Value *result_comm) {
  std::vector<llvm::Value *> results;
  results.push_back(result_src);
  results.push_back(result_tag);
  results.push_back(result_comm);

  return combine_runtime_checks(call, results);
}

llvm::Value *get_runtime_check_result_true(llvm::CallBase *call) {
  IRBuilder<> builder(call);
  return builder.CreateGlobalStringPtr("1");
}

llvm::Value *
combine_runtime_checks(llvm::CallBase *call,
                       const std::vector<llvm::Value *> &check_results) {
  IRBuilder<> builder(call);
  auto &context = call->getContext();

  std::vector<llvm::Value *> no_null_vec;

  std::copy_if(check_results.begin(), check_results.end(),
               std::back_inserter(no_null_vec),
               [](auto *p) { return p != nullptr; });

  if (no_null_vec.empty()) {
    return builder.CreateGlobalStringPtr("");
  }

  for (auto r : check_results) {
    assert(r->getType() == Type::getInt8Ty(context));
  }

  // TODO TEST

  auto result = builder.CreateAlloca(
      Type::getInt8Ty(context),
      ConstantInt::get(IntegerType::getInt8Ty(context), 2));
  // empty string 2 times the null terminator
  builder.CreateStore(ConstantInt::get(IntegerType::getInt16Ty(context), 0),
                      result);

  Value *combined = *no_null_vec.begin();
  if (no_null_vec.size() > 1) {
    combined = builder.CreateOr(no_null_vec);
  }
  builder.CreateStore(combined, result);

  call->getParent()->dump();

  return result;
}