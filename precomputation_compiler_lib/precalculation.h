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
#ifndef MACH_PRECALCULATIONS_H_
#define MACH_PRECALCULATIONS_H_

#include <numeric>
#include <regex>
#include <utility>

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "VtableManager.h"
#include "analysis_results.h"
#include "devirt_analysis.h"
#include "mpi_functions.h"
#include "ptr_info.h"
#include "taintedValue.h"

class PrecalculationAnalysis {
public:
  // TODO documentation
  //  constructor performs the analysis
  //  then generate a slice
  //  than use get_precomputed_value to do what is necessary for the
  //  precomputed values

  // TODO offer helper functions to add calls to precompute backend library

  virtual ~PrecalculationAnalysis() = default;

  // use analysis result to generate the program slice
  virtual void generate_slice();
  // this removes all values in to_precompute_cfg
  virtual void clean_precompute();

  // accessor (use it to modify the program slice if necessary)
  virtual llvm::Value *get_precomputed_value(llvm::Value *v) const;

  // true if F is part of the precompute phase
  virtual bool is_func_part_of_precompute(llvm::Function *F) const;

  // to include the precompute phase in main
  virtual llvm::Function *get_precompute_phase_main() const;
};

// defines the constructor
std::shared_ptr<PrecalculationAnalysis> PrecalculationAnalysisFactroy(
    llvm::Module &M, llvm::Function *entry_point,
    std::vector<llvm::Value *> to_precompute_value,
    std::vector<llvm::Instruction *> to_precompute_cfg);

#endif // MACH_PRECALCULATIONS_H_
