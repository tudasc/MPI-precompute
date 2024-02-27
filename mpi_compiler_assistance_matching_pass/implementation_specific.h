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
#ifndef MACH_IMPLEMENTATION_SPECIFIC_H_
#define MACH_IMPLEMENTATION_SPECIFIC_H_

#include "llvm/IR/Constant.h"
#include "llvm/IR/Module.h"
#include <cassert>

class ImplementationSpecifics {

public:
  // singelton-like
  static ImplementationSpecifics *get_instance() {
    assert(instance != nullptr);
    return instance;
  }

  static ImplementationSpecifics *create_instance(llvm::Module &M) {
    if (instance == nullptr)
      instance = new ImplementationSpecifics(M);
    return instance;
  }

  static void delete_instance() {
    if (instance != nullptr)
      delete instance;
    instance = nullptr;
  }

  // singelton pattern: singeltons are NOT copy-able or assignable
  ImplementationSpecifics(ImplementationSpecifics &other) = delete;

  void operator=(const ImplementationSpecifics &) = delete;

private:
  static ImplementationSpecifics *instance;

  ImplementationSpecifics(llvm::Module &M);

  ~ImplementationSpecifics();

public:
  llvm::GlobalVariable *COMM_WORLD;
  llvm::Constant *ANY_SOURCE;
  llvm::Constant *ANY_TAG;
  llvm::Constant *SUCCESS;

  llvm::Constant *INFO_NULL;
  llvm::Type *mpi_info;

  int get_size_of_mpi_type(llvm::Constant *type);
};

#endif /* MACH_IMPLEMENTATION_SPECIFIC_H_ */
