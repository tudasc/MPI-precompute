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

#ifndef MACH_PTR_INFO_H_
#define MACH_PTR_INFO_H_

#include "taintedValue.h"
#include <limits>
#include <llvm/IR/Instructions.h>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "llvm/IR/Module.h"

#define WILDCARD_IDX std::numeric_limits<unsigned int>::max()

// defined in taintedValue.h
struct TaintedValue;

class PtrUsageInfo : public std::enable_shared_from_this<PtrUsageInfo> {
public:
  explicit PtrUsageInfo(std::shared_ptr<TaintedValue> ptr) {
    if (ptr != nullptr) {
      ptrs_with_this_info.insert(ptr);
    }
  }

  bool isWrittenTo() const {
    assert(is_valid);
    return is_written_to;
  }
  void setIsWrittenTo(bool isWrittenTo) {
    assert(is_valid);
    if ((not is_written_to) && isWrittenTo) {
      propergate_changes();
    }
    is_written_to = is_written_to | isWrittenTo;
  }
  bool isCalled() const {
    assert(is_valid);
    return is_called;
  }
  void setIsCalled(bool isCalled) {
    assert(is_valid);
    if ((not is_called) && isCalled) {
      propergate_changes();
    }
    is_called = is_called | isCalled;
  }
  bool isUsedDirectly() const {
    assert(is_valid);
    return is_used_directly;
  }
  void setIsUsedDirectly(
      bool isUsedDirectly,
      const std::shared_ptr<PtrUsageInfo> &direct_usage_info = nullptr);
  bool isReadFrom() const {
    assert(is_valid);
    return is_read_from;
  }
  void setIsReadFrom(bool isReadFrom) {
    assert(is_valid);
    if ((not is_read_from) && isReadFrom) {
      propergate_changes();
    }
    is_read_from = is_read_from | isReadFrom;
  }
  bool isWholePtrIsRelevant() const {
    assert(is_valid);
    return whole_ptr_is_relevant;
  }
  void setWholePtrIsRelevant(bool wholePtrIsRelevant) {
    assert(is_valid);
    if ((not whole_ptr_is_relevant) && wholePtrIsRelevant) {
      propergate_changes();
    }
    whole_ptr_is_relevant = whole_ptr_is_relevant | wholePtrIsRelevant;
  }
  const std::shared_ptr<PtrUsageInfo> &getInfoOfDirectUsage() const {
    assert(is_valid);
    assert(is_used_directly);
    return info_of_direct_usage;
  };

  void add_ptr_info_user(const std::shared_ptr<TaintedValue> &v) {
    assert(is_valid);
    assert(v != nullptr);
    assert(v->ptr_info == shared_from_this());
    ptrs_with_this_info.insert(v);
  }

  bool is_member_relevant(llvm::GetElementPtrInst *gep);
  void add_important_member(llvm::GetElementPtrInst *gep,
                            const std::shared_ptr<PtrUsageInfo> &result_ptr);

private:
  void add_important_member(std::vector<unsigned int> member_idx,
                            const std::shared_ptr<PtrUsageInfo> &result_ptr);

public:
  void merge_with(std::shared_ptr<PtrUsageInfo> other);

  // for debugging
  void dump();

private:
  // propergates changes by re-visiting all users of this ptr if something
  // important has changed
  void propergate_changes();
  bool is_used_directly = false;
#ifndef NDEBUG
  bool is_valid = true;
#endif

  // returns bool true, if wildcard usage was found
  // false if the known usage has no wildcard
  std::pair<bool, std::shared_ptr<PtrUsageInfo>>
  find_info_for_gep_idx(const std::vector<unsigned int> &member_idx);

  // meaning gep idx 0,0,...(as many zeros as
  // deepest struct nesting level)
  std::shared_ptr<PtrUsageInfo> info_of_direct_usage = nullptr;
  // null if the direct load is a value
  std::shared_ptr<PtrUsageInfo> direct_usage_parent = nullptr;

  std::map<std::vector<unsigned int>, std::shared_ptr<PtrUsageInfo>>
      important_members;

  bool is_read_from = false;
  bool is_written_to = false;
  bool whole_ptr_is_relevant = false; // if accessed in a non-constant gep
  bool is_called = false;

  std::set<std::shared_ptr<PtrUsageInfo>> parents;
  std::set<std::shared_ptr<TaintedValue>>
      ptrs_with_this_info; // all possibly aliasing pointers
};

#endif // MACH_PTR_INFO_H_