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

#include "llvm/IR/Module.h"

#ifndef MACH_PRECALCULATIONS_H_
#define MACH_PRECALCULATIONS_H_

class Precalculations {
public:
  Precalculations(llvm::Module &M, llvm::Function *entry_point)
      : M(M), entry_point(entry_point){};

  void add_precalculations(std::vector<llvm::Value *> to_precompute);

private:
  llvm::Module &M;
  llvm::Function *entry_point;
};

void add_precalculations(llvm::Module &M, llvm::Function *entry_point);

#endif // MACH_PRECALCULATIONS_H_