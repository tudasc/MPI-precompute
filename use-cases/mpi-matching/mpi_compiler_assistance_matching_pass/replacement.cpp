/*
Copyright 2023 Tim Jammer

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
#include "CompilerPassConstants.h"
#include "Precompute_insertion.h"
#include "analysis_results.h"

#include "implementation_specific.h"
#include "mpi_functions.h"
#include "mpiopt_functions.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "debug.h"
#include "nc_settings.h"
#include "precompute_backend_funcs.h"

using namespace llvm;

bool add_init(llvm::Module &M) {

  bool result = false;
  if (get_mpi_functions(M)->mpi_init) {
    assert(get_mpiopt_functions(M)->init);
    for (auto *u : get_mpi_functions(M)->mpi_init->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {

        auto *insert_pt = call->getNextNode();
        if (auto *invoke = dyn_cast<InvokeInst>(call)) {
          insert_pt = invoke->getNormalDest()->getFirstNonPHI();
        }

        IRBuilder<> builder(insert_pt);
        builder.CreateCall(get_mpiopt_functions(M)->init);
        result = true;
      }
    }
  }

  if (get_mpi_functions(M)->mpi_init_thread) {
    assert(get_mpiopt_functions(M)->init);
    for (auto *u : get_mpi_functions(M)->mpi_init_thread->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {

        auto *insert_pt = call->getNextNode();
        if (auto *invoke = dyn_cast<InvokeInst>(call)) {
          insert_pt = invoke->getNormalDest()->getFirstNonPHI();
        }

        IRBuilder<> builder(insert_pt);
        builder.CreateCall(get_mpiopt_functions(M)->init);
        result = true;
      }
    }
  }
  return result;
}

bool add_finalize(llvm::Module &M) {
  bool result = false;
  if (get_mpi_functions(M)->mpi_finalize) {
    assert(get_mpiopt_functions(M)->finalize);
    for (auto *u : get_mpi_functions(M)->mpi_finalize->users()) {
      if (auto *call = dyn_cast<CallBase>(u)) {
        IRBuilder<> builder(call);
        builder.CreateCall(get_mpiopt_functions(M)->finalize);
        result = true;
      }
    }
  }
  return result;
}

void replace_call(CallBase *call, Function *func) {

  assert(call->getFunctionType() == func->getFunctionType());

  call->setCalledFunction(func);
}

void replace_init_call(llvm::CallBase *call, llvm::Function *func) {
  auto mpi_func = get_mpi_functions(*func->getParent());

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

  auto strings = StringConstants::get_instance(call->getModule());

  // set a key value pair to the info object:
  auto key = strings->get_string_ptr("nc_send_strategy");
  auto value = strings->get_string_ptr(STRATEGY);
  builder.CreateCall(mpi_func->mpi_info_set->getFunctionType(),
                     mpi_func->mpi_info_set, {info_obj, key, value});

  key = strings->get_string_ptr("nc_mixed_threshold");
  char threshold_str[30];
  sprintf(threshold_str, "%d", THRESHOLD);
  value = strings->get_string_ptr(threshold_str);
  builder.CreateCall(mpi_func->mpi_info_set->getFunctionType(),
                     mpi_func->mpi_info_set, {info_obj, key, value});

  // enable skipping of matching
  /*
  key = strings->get_string_ptr("skip_matching");
  builder.CreateCall(mpi_func->mpi_info_set->getFunctionType(),
                     mpi_func->mpi_info_set,
                     {info_obj, key, runtime_check_result});
*/
  for (unsigned int i = 0; i < call->arg_size(); ++i) {
    args.push_back(call->getArgOperand(i));
  }
  args.push_back(info_obj);

  CallBase *new_call = nullptr;
  if (auto *invoke = dyn_cast<InvokeInst>(call)) {
    new_call = builder.CreateInvoke(func->getFunctionType(), func,
                                    invoke->getNormalDest(),
                                    invoke->getUnwindDest(), args);

    builder.SetInsertPoint(invoke->getNormalDest()->getFirstNonPHI());
  } else {
    new_call = builder.CreateCall(func->getFunctionType(), func, args);
  }
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
  auto mpi_func = get_mpi_functions(M);

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
  auto mpi_func = get_mpi_functions(M);
  auto mpiopt_functions = get_mpiopt_functions(M);

  // do the actual replacement
  for (auto *call : calls_to_replace) {
    if (call->getCalledFunction() == mpi_func->mpi_wait) {
      replace_call(call, mpiopt_functions->mpi_wait);
    } else if (call->getCalledFunction() == mpi_func->mpi_waitall) {
      replace_call(call, mpiopt_functions->mpi_waitall);
    } else if (call->getCalledFunction() == mpi_func->mpi_waitany) {
      replace_call(call, mpiopt_functions->mpi_waitany);
    } else if (call->getCalledFunction() == mpi_func->mpi_waitsome) {
      replace_call(call, mpiopt_functions->mpi_waitsome);

    } else if (call->getCalledFunction() == mpi_func->mpi_test) {
      replace_call(call, mpiopt_functions->mpi_test);
    } else if (call->getCalledFunction() == mpi_func->mpi_testall) {
      replace_call(call, mpiopt_functions->mpi_testall);
    } else if (call->getCalledFunction() == mpi_func->mpi_testany) {
      replace_call(call, mpiopt_functions->mpi_testany);
    } else if (call->getCalledFunction() == mpi_func->mpi_testsome) {
      replace_call(call, mpiopt_functions->mpi_testsome);

    } else if (call->getCalledFunction() == mpi_func->mpi_start) {
      replace_call(call, mpiopt_functions->mpi_start);
    } else if (call->getCalledFunction() == mpi_func->mpi_startall) {
      replace_call(call, mpiopt_functions->mpi_startall);

    } else if (call->getCalledFunction() == mpi_func->mpi_request_free) {
      replace_call(call, mpiopt_functions->mpi_request_free);

    } else {

      errs() << "This MPI call is currently NOT supported\n";
      call->dump();

      assert(false);
    }
  }
}

std::shared_ptr<StringConstants> StringConstants::instance = nullptr;

llvm::Constant *StringConstants::get_string_ptr(const std::string &s) {
  if (strings_used.find(s) != strings_used.end()) {
    return strings_used[s];
  }
  IRBuilder<> builder(M->getContext());

  auto val = builder.CreateGlobalStringPtr(s, "", 0, M);
  strings_used[s] = val;
  return val;
}

void replace_MPI_with_precompute(
    const std::shared_ptr<PrecomputeInsertion> &precompute,
    struct mpi_functions *mpi_func,
    const std::vector<llvm::CallBase *> &init_calls) {
  for (auto *old_call : init_calls) {
    const bool is_send = is_send_function(old_call->getCalledFunction());
    auto *old_tag = get_tag_value(old_call, is_send);
    auto *old_src = get_src_value(old_call, is_send);

    // TODO this map is currently not build correctly
    auto *precomputed_call = precompute->get_precomputed_value(old_call);

    auto *precomputed_tag = precompute->get_precomputed_value(old_tag);
    auto *precomputed_src = precompute->get_precomputed_value(old_src);

    assert(precomputed_tag ==
           get_tag_value(cast<CallBase>(precomputed_call), is_send));
    assert(precomputed_src ==
           get_src_value(cast<CallBase>(precomputed_call), is_send));

    assert(precomputed_call != nullptr && precomputed_tag != nullptr &&
           precomputed_src != nullptr);
    assert(isa<CallBase>(precomputed_call));

    IRBuilder<> builder = IRBuilder<>(cast<Instruction>(precomputed_call));

    int precompute_envelope_dest;
    int precompute_envelope_tag;
    if (old_call->getCalledFunction() == mpi_func->mpi_send_init) {
      precompute_envelope_dest = SEND_ENVELOPE_DEST;
      precompute_envelope_tag = SEND_ENVELOPE_TAG;
    } else {
      assert(old_call->getCalledFunction() == mpi_func->mpi_recv_init);
      precompute_envelope_dest = RECV_ENVELOPE_DEST;
      precompute_envelope_tag = RECV_ENVELOPE_TAG;
    }

    builder.CreateCall(
        PrecomputeFunctions::get_instance()->register_precomputed_value,
        {builder.getInt32(precompute_envelope_dest), precomputed_src});

    builder.CreateCall(
        PrecomputeFunctions::get_instance()->register_precomputed_value,
        {builder.getInt32(precompute_envelope_tag), precomputed_tag});
  }
}

void add_call_to_precalculation_to_main(
    llvm::Module &M, llvm::CallBase *call_to_init,
    llvm::Function *entry_function,
    const std::shared_ptr<PrecomputeInsertion> &precompute) {

  // insert after init
  IRBuilder<> builder(call_to_init->getNextNode());

  // forward args of main
  std::vector<Value *> args;
  for (auto &arg : entry_function->args()) {
    args.push_back(&arg);
  }
  builder.CreateCall(precompute->get_precompute_main(), args);
}
