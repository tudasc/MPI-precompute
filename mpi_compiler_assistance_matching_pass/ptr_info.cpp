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
    bool isUsedDirectly,
    const std::shared_ptr<PtrUsageInfo> &direct_usage_info) {
  if (not isUsedDirectly) {
    assert(false);
    return;
  }

  if (not is_used_directly) {
    is_used_directly = true;
    // re-visit all users of ptr as something has changed
    for (auto tv : ptrs_with_this_info) {
      tv->visited = false;
    }
  }
  if (direct_usage_info) {
    if (info_of_direct_usage) {
      info_of_direct_usage->merge_with(direct_usage_info);
    } else
      info_of_direct_usage = direct_usage_info;
  }
}

void PtrUsageInfo::merge_with(std::shared_ptr<PtrUsageInfo> other) {
  assert(other != shared_from_this());

  // merge users
  for (const auto &ptr : other->ptrs_with_this_info) {
    assert(ptr->ptr_info != shared_from_this());
    ptr->ptr_info = shared_from_this();
    ptrs_with_this_info.insert(ptr);
  }

  if (other->is_used_directly) {
    this->setIsUsedDirectly(true, other->info_of_direct_usage);
  }

  bool changed = (this->is_read_from != other->is_read_from ||
                  this->is_written_to != other->is_written_to ||
                  this->whole_ptr_is_relevant != other->whole_ptr_is_relevant);

  this->is_read_from = this->is_read_from || other->is_read_from;
  this->is_written_to = this->is_written_to || other->is_written_to;
  this->whole_ptr_is_relevant =
      this->whole_ptr_is_relevant || other->whole_ptr_is_relevant;

  std::move(other->parents.begin(), other->parents.end(),
            std::inserter(parents, parents.end()));

  // merge important_members
  for (auto pos : other->important_members) {
    if (important_members.find(pos.first) != important_members.end()) {
      // may need to merge the information
      if (pos.second != important_members[pos.first]) {
        important_members[pos.first]->merge_with(pos.second);
      } // otherwise it is the same anyway
    } else {
      important_members.insert(pos);
      changed = true;
    }
  }
  if (changed) {
    propergate_changes();
  }
}
void PtrUsageInfo::add_important_member(
    std::vector<unsigned int> member_idx,
    std::shared_ptr<PtrUsageInfo> result_ptr) {

  if (important_members.find(member_idx) != important_members.end()) {
    important_members[member_idx]->merge_with(result_ptr);
    // TODO if merge does not change anything: nothing to do
    propergate_changes();
  } else {
    important_members[member_idx] = result_ptr;
    propergate_changes();
  }
}
void PtrUsageInfo::propergate_changes() {
  // re-visit all users of ptr as something has changed
  for (const auto &tv : ptrs_with_this_info) {
    tv->visited = false;
  }
}
