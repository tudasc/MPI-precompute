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
#ifndef MACH_REPLACEMENT_H_
#define MACH_REPLACEMENT_H_

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"

#include <map>
#include <vector>

// singelton
class StringConstants {

private:
  explicit StringConstants(llvm::Module *M) : M(M) {}
  static std::shared_ptr<StringConstants> instance;

public:
  static std::shared_ptr<StringConstants> get_instance(llvm::Module *M) {
    if (instance == nullptr) {
      // allow make_shared to call the private constructor
      struct make_shared_enabler : public StringConstants {
        make_shared_enabler(llvm::Module *M)
            : StringConstants(std::forward<llvm::Module *>(M)) {}
      };
      instance = std::make_shared<make_shared_enabler>(M);
    }
    return instance;
  }

  llvm::Constant *get_string_ptr(const std::string &s);

private:
  std::map<std::string, llvm::Constant *> strings_used;
  llvm::Module *M;
};

// true if something changed
bool add_init(llvm::Module &M);
bool add_finalize(llvm::Module &M);

void replace_request_handling_calls(llvm::Module &M);

void replace_init_call(llvm::CallBase *call, llvm::Function *func);

#endif /* MACH_REPLACEMENT_H_ */
