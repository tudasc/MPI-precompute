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

#include "ptr_info.h"
#include "analysis_results.h"
#include "conflict_detection.h"
#include "devirt_analysis.h"
#include "mpi_functions.h"
#include "taintedValue.h"
#include <cassert>

#include "implementation_specific.h"
#include "mpi_functions.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/IR/Verifier.h"

#include "debug.h"
using namespace llvm;

void PtrUsageInfo::setIsUsedDirectly(
    bool isUsedDirectly, std::shared_ptr<PtrUsageInfo> direct_usage_info) {
  // TODO implement
  assert(false);
}

void PtrUsageInfo::merge_with(std::shared_ptr<PtrUsageInfo> other) {
  // TODO implement
  assert(false);
}
