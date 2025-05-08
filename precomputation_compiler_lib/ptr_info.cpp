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
#include "ptr_info.h"
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
      // info of direct usage may be this if whole_derived is relevant
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
  if (_other == nullptr) {
    return; // no-op
  }

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
    auto reference_to_this = shared_from_this();
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
         this->whole_ptr_is_relevant != other->whole_ptr_is_relevant ||
         this->is_derived_ptr_relevant != other->is_derived_ptr_relevant);

    this->is_read_from = this->is_read_from || other->is_read_from;
    this->is_written_to = this->is_written_to || other->is_written_to;
    this->whole_ptr_is_relevant =
        this->whole_ptr_is_relevant || other->whole_ptr_is_relevant;

    this->is_derived_ptr_relevant =
        this->is_derived_ptr_relevant || other->is_derived_ptr_relevant;

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
    bool need_merging_gep = this->is_derived_ptr_relevant;

    // this may be invalidated (if gep is result of self)
    for (const auto &pos : other->important_members) {
      // descent further if this was invalidated
      while (reference_to_this->merged_with != nullptr) {
        reference_to_this = reference_to_this->merged_with;
      }

      // set correct gep type
      bool need_pad = false;
      // merge gep type
      if (other->gep_type && !reference_to_this->gep_type) {
        reference_to_this->gep_type = other->gep_type;
      } else if (reference_to_this->gep_type && other->gep_type) {
        if (reference_to_this->gep_type != other->gep_type) {
          need_pad = reference_to_this->need_pad_for_gep(other->gep_type);
        } // else nothing to do: already same
      }
      auto idxs = pos.first;
      if (need_pad) {
        // insert pad if needed
        idxs.insert(idxs.begin(), 0);
      }
      if (need_merging_gep) {
        reference_to_this->merge_with(pos.second);
      } else {
        // this will propagate changes if applicable
        reference_to_this->add_important_member(idxs, pos.second);
      }
    }

    // we can clean up other, as other is only used to forward to this by now
    other->ptrs_with_this_info.clear();
    other->important_members.clear();
    other->info_of_direct_usage = nullptr;
  }
}

// pad= true: add additional 0 in front to handle acces from basetype instead of
// array type
std::vector<long> get_gep_idxs(llvm::GetElementPtrInst *gep, bool pad) {
  std::vector<long> idxs;
  if (pad) {
    idxs.push_back(0);
  }
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
    const std::shared_ptr<PtrUsageInfo> &result_ptr_) {
  if (merged_with) {
    merged_with->add_important_member(gep, result_ptr_);
    return;
  }

  assert(is_valid);
  if (result_ptr_) {
    std::shared_ptr<PtrUsageInfo> result_ptr = result_ptr_;
    while (result_ptr->merged_with != nullptr) {
      // decent
      result_ptr = result_ptr->merged_with;
    }

    if (result_ptr == shared_from_this()) {
      return; // nothing to do
      // e.g. an iterator where it++ is realized as a GEP instruction
    }
    if (is_derived_ptr_relevant) {
      shared_from_this()->merge_with(result_ptr);
      return;
    }
  }

  if (!gep_type) {
    assert(important_members.empty());
    gep_type = gep->getSourceElementType();
  }

  auto member_idx =
      get_gep_idxs(gep, need_pad_for_gep(gep->getSourceElementType()));
  add_important_member(member_idx, result_ptr_);
}

bool PtrUsageInfo::need_pad_for_gep(llvm::Type *type_of_gep) {
  if (this->gep_type == nullptr) {
    return false;
  }
  if (this->gep_type == type_of_gep) {
    return false;
  }
  // or type sizes are same
  auto DL = M->getDataLayout();
  if (DL.getTypeAllocSize(this->gep_type) == DL.getTypeAllocSize(type_of_gep)) {
    return false;
  }
  // cast to void*
  if (type_of_gep == Type::getInt8Ty(M->getContext()) ||
      type_of_gep == PointerType::get(M->getContext(), 0) ||
      gep_type == Type::getInt8Ty(M->getContext()) ||
      gep_type == PointerType::get(M->getContext(), 0)) {
    // this may happen if ptr is cast to void* e.g. passed to memset call
    // in this case we dont know which geps will alias
    whole_ptr_is_relevant = true;

    return false;
  }

  if (auto this_array_type = dyn_cast<ArrayType>(this->gep_type)) {

    if (this_array_type->getElementType() == type_of_gep) {
      return true;
    }
  }

  if (auto other_array_type = dyn_cast<ArrayType>(type_of_gep)) {
    // in this case we need to raise the type of our gep type to the array type
    assert(other_array_type->getElementType() == this->gep_type);
    this->gep_type = type_of_gep;
    // pre-pend 0 to all existing gep members (need to copy whole map)
    std::map<std::vector<long>, std::shared_ptr<PtrUsageInfo>>
        old_important_members(important_members);
    important_members.clear();
    for (const auto &[key, value] : old_important_members) {

      std::vector<long> new_key = {0};
      std::copy(key.begin(), key.end(), std::back_inserter(new_key));
      important_members[new_key] = value;
    }

    return false;
  }
  gep_type->dump();
  type_of_gep->dump();

  if (gep_type->isStructTy() && type_of_gep->isStructTy()) {
    // "different" classes e.g. one is base and the other is derived
    // no need to take special care
    return false;
  }

  // other cases, our alias analysis will need to assume all ptr based on this
  // gep may alias
  whole_ptr_is_relevant = true;
  return false;

  errs() << "\n\n";
  this->gep_type->dump();
  errs() << "\n\n";
  type_of_gep->dump();
  errs() << "\n\n";

  assert(false && "not supported yet");

  return false;
}

void PtrUsageInfo::add_important_member(
    std::vector<long> member_idx,
    const std::shared_ptr<PtrUsageInfo> &result_ptr) {
  assert(is_valid);
  assert(this->merged_with == nullptr);
  assert(gep_type); // need to set gep type first

  auto to_propergate = shared_from_this();
  // this can go out of scope so we need to capture a shared ptr to it first

  // auto reference_to_self = shared_from_this();

  // we don't keep track of if the GEP results in ptr again
  if (result_ptr == shared_from_this()) {
    return; // nothing to do
    // e.g. an iterator where it++ is realized as a GEP instruction
  }

  bool has_changed = false;
  auto existing_info = find_info_for_gep_idx(member_idx);

  if (existing_info.second == nullptr) {
    important_members[member_idx] = result_ptr;
    has_changed = true;

  } else { // info already present
    if (not existing_info.first && member_idx[member_idx.size() - 1]) {
      // new usage has wildcard but old usages may not
      // we need to combine all usages that match this wildcard
      has_changed = true;

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
      if (existing_info.second != result_ptr) {
        has_changed = true;
      }
      existing_info.second->merge_with(result_ptr);
    }
  }

  // this may be invalid if it needs to be merged due to an iterator++ being
  // realized as GEP

  while (to_propergate->merged_with) {
    to_propergate = to_propergate->merged_with;
  }
  if (has_changed) {
    to_propergate->propergate_changes();
  }
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

void PtrUsageInfo::setDerivedPtrIsRelevant(bool derived_relevant) {
  if (merged_with) {
    merged_with->setDerivedPtrIsRelevant(derived_relevant);
    return;
  }
  assert(is_valid);
  if ((not is_derived_ptr_relevant) && derived_relevant) {
    is_derived_ptr_relevant = true;
    this->setWholePtrIsRelevant(true);
    auto reference_to_this =
        shared_from_this(); // may be invalidated when merging

    // may be invalidated when merging, need to collect ptrs first
    std::vector<std::shared_ptr<PtrUsageInfo>> to_merge;

    if (info_of_direct_usage) {
      to_merge.push_back(info_of_direct_usage);
    }
    for (auto &pair : important_members) {
      to_merge.push_back(pair.second);
    }

    for (auto &m : to_merge) {
      reference_to_this->merge_with(m);
      // descent further if this was invalidated
      while (reference_to_this->merged_with != nullptr) {
        reference_to_this = reference_to_this->merged_with;
      }
    }

    reference_to_this->propergate_changes();
  }
  // else nothing to do
}

bool PtrUsageInfo::is_member_relevant(llvm::GetElementPtrInst *gep) {
  if (merged_with) {
    return merged_with->is_member_relevant(gep);
  }
  assert(is_valid);

  return whole_ptr_is_relevant ||
         find_info_for_gep_idx(
             get_gep_idxs(gep, need_pad_for_gep(gep->getSourceElementType())))
                 .second != nullptr;
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
  errs() << "Aliasing ptrs:\n";
  for (const auto &u : ptrs_with_this_info) {
    errs() << "\t";
    if (auto *func = dyn_cast<Function>(u.lock()->v)) {
      errs() << "@" << func->getName();
    } else {
      u.lock()->v->dump();
    }
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
  errs() << "Is whole derived ptr relevant : " << is_derived_ptr_relevant
         << "\n";
  errs() << "Important GEP members : \n";
  for (auto pair : important_members) {
    errs() << "\t";
    for (auto idx : pair.first) {
      errs() << idx << ", ";
    }
    errs() << "\n";

    // if (pair.second!=shared_from_this()) {
    //      pair.second->dump();
    //    }
  }
  errs() << "End PtrUsageInfo\n";
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
  assert(llvm::isa<llvm::StoreInst>(store) ||
         llvm::isa<llvm::CallBase>(store) ||
         llvm::isa<llvm::AtomicRMWInst>(store));
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
  assert(llvm::isa<llvm::LoadInst>(load) || llvm::isa<llvm::CallBase>(load) ||
         llvm::isa<AtomicRMWInst>(load));
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
  if (ll) {
    while (ll->merged_with) {
      ll = ll->merged_with;
    }
  }
  auto rr = rhs;
  if (rr) {
    while (rr->merged_with) {
      rr = rr->merged_with;
    }
  }
  return ll.get() == rr.get();
}

template <>
bool std::operator!=(const std::shared_ptr<PtrUsageInfo> &lhs,
                     const std::shared_ptr<PtrUsageInfo> &rhs) noexcept {

  auto ll = lhs;
  if (ll) {
    while (ll->merged_with) {
      ll = ll->merged_with;
    }
  }
  auto rr = rhs;
  if (rr) {
    while (rr->merged_with) {
      rr = rr->merged_with;
    }
  }
  return ll.get() != rr.get();
}

template <>
bool std::operator<(const std::shared_ptr<PtrUsageInfo> &lhs,
                    const std::shared_ptr<PtrUsageInfo> &rhs) noexcept {

  auto ll = lhs;
  if (ll) {
    while (ll->merged_with) {
      ll = ll->merged_with;
    }
  }
  auto rr = rhs;
  if (rr) {
    while (rr->merged_with) {
      rr = rr->merged_with;
    }
  }
  return ll.get() < rr.get();
}

template <>
bool std::operator>(const std::shared_ptr<PtrUsageInfo> &lhs,
                    const std::shared_ptr<PtrUsageInfo> &rhs) noexcept {

  auto ll = lhs;
  if (ll) {
    while (ll->merged_with) {
      ll = ll->merged_with;
    }
  }
  auto rr = rhs;
  if (rr) {
    while (rr->merged_with) {
      rr = rr->merged_with;
    }
  }
  return ll.get() > rr.get();
}

template <>
bool std::operator<=(const std::shared_ptr<PtrUsageInfo> &lhs,
                     const std::shared_ptr<PtrUsageInfo> &rhs) noexcept {

  auto ll = lhs;
  if (ll) {
    while (ll->merged_with) {
      ll = ll->merged_with;
    }
  }
  auto rr = rhs;
  if (rr) {
    while (rr->merged_with) {
      rr = rr->merged_with;
    }
  }
  return ll.get() <= rr.get();
}

template <>
bool std::operator>=(const std::shared_ptr<PtrUsageInfo> &lhs,
                     const std::shared_ptr<PtrUsageInfo> &rhs) noexcept {

  auto ll = lhs;
  if (ll) {
    while (ll->merged_with) {
      ll = ll->merged_with;
    }
  }
  auto rr = rhs;
  if (rr) {
    while (rr->merged_with) {
      rr = rr->merged_with;
    }
  }
  return ll.get() >= rr.get();
}
