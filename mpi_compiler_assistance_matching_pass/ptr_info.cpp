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

#include <boost/stacktrace.hpp>
#include <iostream>

#include "debug.h"
using namespace llvm;

void PtrUsageInfo::setIsUsedDirectly(
    bool isUsedDirectly,
    const std::shared_ptr<PtrUsageInfo> &direct_usage_info) {
  assert(is_valid);
  if (not is_used_directly) {
    is_used_directly = true;
    propergate_changes();
  }

  if (direct_usage_info) {
    if (info_of_direct_usage) {
      info_of_direct_usage->merge_with(direct_usage_info);
      // merge will propergate changes if any
    } else
      info_of_direct_usage = direct_usage_info;
  }
}

void PtrUsageInfo::merge_with(std::shared_ptr<PtrUsageInfo> other) {
  assert(other != nullptr);

  if (not is_valid) {
    errs() << "Invalid: " << shared_from_this().get() << "\n";
  }

  assert(is_valid);
  if (other != shared_from_this()) {
    if (not other->is_valid) {
      errs() << "Invalid: " << other.get() << "\n";
    }
    assert(other->is_valid);

    // if other == shared_from_this(): nothing to do already the same ptr info

    // merge users
    for (const auto &ptr : other->ptrs_with_this_info) {
      assert(ptr->ptr_info == other);
      assert(ptr->ptr_info != shared_from_this());
      ptr->ptr_info = shared_from_this();
      ptrs_with_this_info.insert(ptr);
    }
    // merge parents
    for (auto &parent : other->parents) {
      int count_parents = 0;
      for (const auto &pair : parent->important_members) {
        if (pair.second == other) {
          count_parents++;
          auto gep_idx = pair.first;
          parent->important_members[gep_idx] = shared_from_this();
          parents.insert(parent);
        }
      }
      assert(count_parents > 0);
    }

    if (other->is_used_directly) {
      this->setIsUsedDirectly(true, other->info_of_direct_usage);
      // will merge the info_of_direct_usage
    }

    bool changed =
        (this->is_read_from != other->is_read_from ||
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
      // TODO wildcard usage!
      if (important_members.find(pos.first) != important_members.end()) {
        // merge the information
        important_members[pos.first]->merge_with(pos.second);
      } else {
        important_members.insert(pos);
        changed = true;
      }
    }
    if (changed) {
      propergate_changes();
    }

    // TODO no one should be able to retain a reference to other
    // assert(other.use_count()==1);
    errs() << "use_count of other: " << other.use_count() << "\n";
#ifndef NDEBUG
    other->is_valid = false;
    errs() << "Invalidate:\n";
    auto info = *other->ptrs_with_this_info.begin();
    info->v->dump();
    errs() << other.get() << "\n";
    std::stringstream stacktrace_stream;
    stacktrace_stream << boost::stacktrace::stacktrace();
    errs() << stacktrace_stream.str() << "\n";
#endif
    for (const auto &u : ptrs_with_this_info) {
      assert(u->ptr_info != other);
    }
  }
}

std::vector<unsigned int> get_gep_idxs(llvm::GetElementPtrInst *gep) {
  std::vector<unsigned int> idxs;
  for (auto &idx : gep->indices()) {
    auto idx_constant = dyn_cast<ConstantInt>(&idx);
    if (idx_constant) {
      unsigned int idx_v = idx_constant->getZExtValue();
      idxs.push_back(idx_v);
    } else {
      idxs.push_back(WILDCARD_IDX);
      break;
    }
  }
  return idxs;
}

bool is_member_matching(const std::vector<unsigned int> &member_idx,
                        const std::vector<unsigned int> &member_idx_reference) {
  if (member_idx.size() > member_idx_reference.size()) {
    // swap args so that we can assume the right one is larger
    return is_member_matching(member_idx_reference, member_idx);
  }
  assert(member_idx.size() <= member_idx_reference.size());

  for (size_t i = 0; i < member_idx_reference.size(); ++i) {
    auto idx = member_idx_reference[i];
    unsigned int member = 0; // implicit 0 if the other one runs out of indices
    if (i < member_idx.size()) {
      member = member_idx[i];
    }
    if ((idx == WILDCARD_IDX) || (member == WILDCARD_IDX)) {
      return true;
    }
    if (idx != member) {
      return false;
    }
  }
  return true;
}

void PtrUsageInfo::add_important_member(
    llvm::GetElementPtrInst *gep, std::shared_ptr<PtrUsageInfo> result_ptr) {
  assert(is_valid);

  auto member_idx = get_gep_idxs(gep);

  auto existing_info = find_info_for_gep_idx(member_idx);

  if (existing_info.second == nullptr) {
    important_members[member_idx] = result_ptr;
    result_ptr->parents.insert(shared_from_this());

  } else { // info already present
    if (not existing_info.first && member_idx[member_idx.size() - 1]) {
      // new usage has wildcard but old usages may not
      // we need to combine all usages that match this wildcard

      std::set<std::shared_ptr<PtrUsageInfo>> to_merge;
      // the set removes duplicates
      for (const auto &pair : important_members) {
        if (is_member_matching(member_idx, pair.first)) {
          to_merge.insert(pair.second);
        }
      }
      for (auto &m : to_merge) {
        // a ptr may not be its own GEP result
        assert(m != shared_from_this());
        result_ptr->merge_with(m);
      }

      // std::erase_if in c++20
      for (auto iter = important_members.begin();
           iter != important_members.end();) {
        if (is_member_matching(member_idx, iter->first)) {
          iter = important_members.erase(iter);
        } else {
          ++iter;
        }
      }
      important_members[member_idx] = result_ptr;
      result_ptr->parents.insert(shared_from_this());

    } else {
      // exact match regarding wildcards
      existing_info.second->merge_with(result_ptr);
    }
  }
  // TODO if merge does not change anything: nothing to do
  // but propergate "changes" is not wrong in either case
  propergate_changes();
}

void PtrUsageInfo::propergate_changes() {
  assert(is_valid);
  // re-visit all users of ptr as something has changed
  for (const auto &tv : ptrs_with_this_info) {
    tv->visited = false;
  }
  // TODO do we need to re-visit all the parents or childrens as well?
}

bool PtrUsageInfo::is_member_relevant(llvm::GetElementPtrInst *gep) {
  assert(is_valid);
  return find_info_for_gep_idx(get_gep_idxs(gep)).second != nullptr;
}

std::pair<bool, std::shared_ptr<PtrUsageInfo>>
PtrUsageInfo::find_info_for_gep_idx(
    const std::vector<unsigned int> &member_idx) {
  assert(is_valid);
  auto pos = std::find_if(important_members.begin(), important_members.end(),
                          [&member_idx](auto pair) {
                            auto idxs = pair.first;
                            return is_member_matching(member_idx, idxs);
                          });
  if (pos != important_members.end()) {
    return std::make_pair(pos->first[pos->first.size() - 1] == WILDCARD_IDX,
                          pos->second);
  } else {
    return std::make_pair(false, nullptr);
  }
}

void PtrUsageInfo::dump() {

  errs() << "PtrUsageInfo:\n";
#ifndef NDEBUG
  if (not is_valid) {
    errs() << "INVALID\n";
  }
#endif
  errs() << "Users:\n";
  for (const auto &u : ptrs_with_this_info) {
    errs() << "\t";
    u->v->dump();
  }
  errs() << "Is Read : " << is_read_from << "\n";
  errs() << "Is Written : " << is_written_to << "\n";
  errs() << "Used Directly : " << is_used_directly << "\n";
  errs() << "Is Called : " << is_called << "\n";
  errs() << "Is whole ptr relevant : " << whole_ptr_is_relevant
         << " (non-constant-gep)\n";
  errs() << "Important GEP members : \n";
  for (auto pair : important_members) {
    errs() << "\t";
    for (auto idx : pair.first) {
      errs() << idx << ", ";
    }
    errs() << "\n";
    pair.second->dump();
  }
}
