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
#include "mpi_functions.h"
#include "precalculation.h"
#include "taintedValue.h"
#include <cassert>

#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"

#include <iostream>

using namespace llvm;

void PtrUsageInfo::setIsUsedDirectly(
    bool isUsedDirectly, std::shared_ptr<PtrUsageInfo> direct_usage_info) {
  if (merged_with) {
    merged_with->setIsUsedDirectly(isUsedDirectly, direct_usage_info);
    return;
  }
  assert(is_valid);
  assert(isUsedDirectly == true);
  if (not is_used_directly) {
    is_used_directly = true;
    propergate_changes();
  }

  // TODO do we need the direct_usage_parent for alias detection
  if (direct_usage_info) {
    auto info_to_use = direct_usage_info;
    while (info_to_use->merged_with != nullptr) {
      info_to_use = info_to_use->merged_with;
    }
    assert(info_to_use->is_valid);

    if (info_of_direct_usage) {
      info_of_direct_usage->merge_with(info_to_use);
      // merge will propergate changes if any
    } else {
      info_of_direct_usage = info_to_use;
    }
  }
}

// other MAY NOT be passed as const ref as we might recursively destruct it
// before we are finish using it
void PtrUsageInfo::merge_with(std::shared_ptr<PtrUsageInfo> _other) { // NOLINT
  if (merged_with) {
    merged_with->merge_with(_other);
    return;
  }

  assert(_other != nullptr);

  auto other = _other;
  while (other->merged_with != nullptr) {
    other = other->merged_with;
  }
#ifndef NDEBUG
  if (not is_valid) {
    errs() << "Invalid: " << shared_from_this().get() << "\n";
  }
#endif
  assert(is_valid);
  assert(this->merged_with == nullptr);

  if (other != shared_from_this()) {
    // if other == shared_from_this(): nothing to do already the same ptr info
#ifndef NDEBUG
    if (not other->is_valid) {
      errs() << "Invalid: " << other.get() << "\n";
    }
#endif
    assert(other->is_valid);
    assert(other->merged_with == nullptr);
    other->merged_with = shared_from_this();
#ifndef NDEBUG
    other->is_valid = false;
#endif
    assert(this->is_valid);
    auto obj_to_merge_to = shared_from_this();
    // this may go out of scope so we capture the shared ptr early in this
    // function

    // merge users
    for (const auto &ptr : other->ptrs_with_this_info) {
      assert(not ptr.expired());
      // assert(ptr->ptr_info == other);
      // assert(ptr->ptr_info != shared_from_this());

      this->ptrs_with_this_info.insert(ptr);
      // directly replace references to other instead of dispatching calls to
      // this
      ptr.lock()->ptr_info = shared_from_this();
    }

    bool changed =
        (this->is_read_from != other->is_read_from ||
         this->is_written_to != other->is_written_to ||
         this->whole_ptr_is_relevant != other->whole_ptr_is_relevant);

    this->is_read_from = this->is_read_from || other->is_read_from;
    this->is_written_to = this->is_written_to || other->is_written_to;
    this->whole_ptr_is_relevant =
        this->whole_ptr_is_relevant || other->whole_ptr_is_relevant;

    for (auto *s : other->stores) {
      auto pair = stores.insert(s);
      changed = changed | pair.second; // changed if a new elem was inserted
    }
    for (auto *s : other->loads) {
      auto pair = loads.insert(s);
      changed = changed | pair.second; // changed if a new elem was inserted
    }

    if (changed) {
      propergate_changes();
    }

    if (other->is_used_directly) {
      this->setIsUsedDirectly(true, other->info_of_direct_usage);
      // will merge the info_of_direct_usage
    }

    // this may be invalidated (if gep is result of self)
    for (const auto &pos : other->important_members) {
      while (obj_to_merge_to->merged_with != nullptr) {
        obj_to_merge_to = obj_to_merge_to->merged_with;
      }
      // this will propagate changes if applicable
      obj_to_merge_to->add_important_member(pos.first, pos.second);
    }

    // we can clean up other, as other is only used to forward to this by now
    other->ptrs_with_this_info.clear();
    other->important_members.clear();
    other->info_of_direct_usage = nullptr;
  }
}

std::vector<long> get_gep_idxs(llvm::GetElementPtrInst *gep) {
  std::vector<long> idxs;
  for (auto &idx : gep->indices()) {
    auto idx_constant = dyn_cast<ConstantInt>(&idx);
    if (idx_constant) {
      long idx_v = idx_constant->getSExtValue();
      idxs.push_back(idx_v);
    } else {
      idxs.push_back(WILDCARD_IDX);
      break;
    }
  }
  return idxs;
}

bool is_member_matching(const std::vector<long> &member_idx,
                        const std::vector<long> &member_idx_reference) {
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
    llvm::GetElementPtrInst *gep,
    const std::shared_ptr<PtrUsageInfo> &result_ptr) {
  if (merged_with) {
    merged_with->add_important_member(gep, result_ptr);
    return;
  }
  assert(is_valid);

  if (result_ptr == shared_from_this()) {
    return; // nothing to do
    // e.g. an iterator where it++ is realized as a GEP instruction
  }

  auto member_idx = get_gep_idxs(gep);
  add_important_member(member_idx, result_ptr);
}

void PtrUsageInfo::add_important_member(
    std::vector<long> member_idx,
    const std::shared_ptr<PtrUsageInfo> &result_ptr) {
  assert(is_valid);
  assert(this->merged_with == nullptr);

  auto to_propergate = shared_from_this();
  // this can go out of scope so we need to capture a shared ptr to it first

  // auto reference_to_self = shared_from_this();

  // we don't keep track of if the GEP results in ptr again
  if (result_ptr == shared_from_this()) {
    return; // nothing to do
    // e.g. an iterator where it++ is realized as a GEP instruction
  }

  auto existing_info = find_info_for_gep_idx(member_idx);

  if (existing_info.second == nullptr) {
    important_members[member_idx] = result_ptr;

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
      // the std:: set still holds references of the shared ptr, so they won't
      // be destroyed

      // merging may invalidate this
      for (auto &m : to_merge) {
        result_ptr->merge_with(m);
      }
    } else {
      // exact match regarding wildcards
      existing_info.second->merge_with(result_ptr);
    }
  }

  // this may be invalid if it needs to be merged due to an iterator++ being
  // realized as GEP

  while (to_propergate->merged_with) {
    to_propergate = to_propergate->merged_with;
  }
  // TODO if merge does not change anything: nothing to do
  // but propergate "changes" is not wrong in either case
  to_propergate->propergate_changes();
}

void PtrUsageInfo::propergate_changes() {
  assert(is_valid);
  assert(merged_with == nullptr);
  // re-visit all users of ptr as something has changed
  for (const auto &tv : ptrs_with_this_info) {
    assert(not tv.expired());
    tv.lock()->visited = false;
  }
}

bool PtrUsageInfo::is_member_relevant(llvm::GetElementPtrInst *gep) {
  if (merged_with) {
    return merged_with->is_member_relevant(gep);
  }
  assert(is_valid);
  return find_info_for_gep_idx(get_gep_idxs(gep)).second != nullptr;
}

std::pair<bool, std::shared_ptr<PtrUsageInfo>>
PtrUsageInfo::find_info_for_gep_idx(const std::vector<long> &member_idx) {
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
  if (merged_with) {
    merged_with->dump();
    return;
  }

  errs() << "PtrUsageInfo:\n";

#ifndef NDEBUG
  if (not is_valid) {
    errs() << "INVALID\n";
  }
#endif
  errs() << "Users:\n";
  for (const auto &u : ptrs_with_this_info) {
    errs() << "\t";
    u.lock()->v->dump();
    errs() << "\t";
    errs() << "\t";
    if (auto *inst = dyn_cast<Instruction>(u.lock()->v)) {
      errs() << "in : " << inst->getFunction()->getName();
    }
    if (auto *arg = dyn_cast<Argument>(u.lock()->v)) {
      errs() << "in : " << arg->getParent()->getName();
    }
    errs() << "\n";
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
    if (pair.first==std::vector<long>{0,2}){
     pair.second->dump();}
  }
}
const std::set<std::weak_ptr<TaintedValue>,
               std::owner_less<std::weak_ptr<TaintedValue>>> &
PtrUsageInfo::getPtrsWithThisInfo() const {
  return ptrs_with_this_info;
}

void PtrUsageInfo::setIsWrittenTo(
    llvm::Instruction *store, const PrecalculationAnalysis *precalc_analysis) {
  if (merged_with) {
    merged_with->setIsWrittenTo(store, precalc_analysis);
    return;
  }
  assert(precalc_analysis);
  assert(is_valid);
  assert(llvm::isa<llvm::StoreInst>(store) || llvm::isa<llvm::CallBase>(store));
  is_written_to = true;
  auto pair = stores.insert(store);
  if (pair.second) { // if it was inserted
    propergate_changes();

    // recursively mark all callsites potentially calling the function as stores
    // as well
    auto func = precalc_analysis->get_function_analysis(store->getFunction());
    for (auto *call : func->callsites) {
      if (call != store) {
        setIsWrittenTo(call, precalc_analysis);
      }
    }
  }
}

void PtrUsageInfo::setIsReadFrom(
    llvm::Instruction *load, const PrecalculationAnalysis *precalc_analysis) {
  if (merged_with) {
    merged_with->setIsReadFrom(load, precalc_analysis);
    return;
  }
  assert(precalc_analysis);
  assert(is_valid);
  assert(llvm::isa<llvm::LoadInst>(load) || llvm::isa<llvm::CallBase>(load));
  is_read_from = true;
  auto pair = loads.insert(load);
  if (pair.second) { // if it was inserted
    propergate_changes();

    // recursively mark all callsites potentially calling the function as stores
    // as well
    auto func = precalc_analysis->get_function_analysis(load->getFunction());
    for (auto *call : func->callsites) {
      if (call != load) {
        setIsReadFrom(call, precalc_analysis);
      }
    }
  }
}
const std::set<llvm::Instruction *> &PtrUsageInfo::getStores() const {
  return stores;
}

const std::set<llvm::Instruction *> &PtrUsageInfo::getLoads() const {
  return loads;
}

template <>
bool std::operator==(const std::shared_ptr<PtrUsageInfo> &lhs,
                     const std::shared_ptr<PtrUsageInfo> &rhs) noexcept {

  auto ll = lhs;
  while (ll->merged_with) {
    ll = ll->merged_with;
  }
  auto rr = rhs;
  while (rr->merged_with) {
    rr = rr->merged_with;
  }
  return ll.get() == rr.get();
}

template <>
bool std::operator!=(const std::shared_ptr<PtrUsageInfo> &lhs,
                     const std::shared_ptr<PtrUsageInfo> &rhs) noexcept {

  auto ll = lhs;
  while (ll->merged_with) {
    ll = ll->merged_with;
  }
  auto rr = rhs;
  while (rr->merged_with) {
    rr = rr->merged_with;
  }
  return ll.get() != rr.get();
}

template <>
bool std::operator<(const std::shared_ptr<PtrUsageInfo> &lhs,
                    const std::shared_ptr<PtrUsageInfo> &rhs) noexcept {

  auto ll = lhs;
  while (ll->merged_with) {
    ll = ll->merged_with;
  }
  auto rr = rhs;
  while (rr->merged_with) {
    rr = rr->merged_with;
  }
  return ll.get() < rr.get();
}

template <>
bool std::operator>(const std::shared_ptr<PtrUsageInfo> &lhs,
                    const std::shared_ptr<PtrUsageInfo> &rhs) noexcept {

  auto ll = lhs;
  while (ll->merged_with) {
    ll = ll->merged_with;
  }
  auto rr = rhs;
  while (rr->merged_with) {
    rr = rr->merged_with;
  }
  return ll.get() > rr.get();
}

template <>
bool std::operator<=(const std::shared_ptr<PtrUsageInfo> &lhs,
                     const std::shared_ptr<PtrUsageInfo> &rhs) noexcept {

  auto ll = lhs;
  while (ll->merged_with) {
    ll = ll->merged_with;
  }
  auto rr = rhs;
  while (rr->merged_with) {
    rr = rr->merged_with;
  }
  return ll.get() <= rr.get();
}

template <>
bool std::operator>=(const std::shared_ptr<PtrUsageInfo> &lhs,
                     const std::shared_ptr<PtrUsageInfo> &rhs) noexcept {

  auto ll = lhs;
  while (ll->merged_with) {
    ll = ll->merged_with;
  }
  auto rr = rhs;
  while (rr->merged_with) {
    rr = rr->merged_with;
  }
  return ll.get() >= rr.get();
}
