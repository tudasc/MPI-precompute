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

#ifndef MACH_REPLACEMENT_H_
#define MACH_REPLACEMENT_H_

#include "llvm/IR/InstrTypes.h"

#include <vector>

// true if something changed
bool add_init(llvm::Module &M);
bool add_finalize(llvm::Module &M);

void replace_request_handling_calls(llvm::Module &M);

void replace_init_call(llvm::CallBase *call, llvm::Function *func,
                       llvm::Value *runtime_check_result);

// returns the llvm value that represents is the result of the runtime check
llvm::Value *insert_runtime_check(llvm::Value *val_a, llvm::Value *val_b);

// returns the llvm value that represents is the result of the runtime check
// always returns true value as result
llvm::Value *get_runtime_check_result_true(llvm::CallBase *call);
// returns the llvm value that represents is the result of the runtime check
// always returns false value as result
llvm::Value *get_runtime_check_result_false(llvm::CallBase *call);

llvm::Value *get_runtime_check_result_str(llvm::CallBase *call,
                                          llvm::Value *check_result);

// returns the llvm value that represents is the result of the runtime check
// values are combined with or
// result values may be nullptr - nullptr are considered 0
// value
llvm::Value *combine_runtime_checks(llvm::CallBase *call,
                                    llvm::Value *result_src,
                                    llvm::Value *result_tag,
                                    llvm::Value *result_comm);

// returns the llvm value that represents is the result of the runtime check
// values are combined with or
// check_results vector may include nullptr - nullptr are considered 0
llvm::Value *
combine_runtime_checks(llvm::CallBase *call,
                       const std::vector<llvm::Value *> &check_results);

#endif /* MACH_REPLACEMENT_H_ */
