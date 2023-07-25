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

#ifndef MACH_CONFLICT_DETECTION_H_
#define MACH_CONFLICT_DETECTION_H_

#include "frontend_plugin_data.h"
#include "llvm/IR/InstrTypes.h"

#include <vector>

class PersistentMPIInitCall
    : public std::enable_shared_from_this<PersistentMPIInitCall> {
public:
  static std::shared_ptr<PersistentMPIInitCall>
  get_PersistentMPIInitCall(llvm::CallBase *init_call);

private:
  static std::map<llvm::CallBase *, std::shared_ptr<PersistentMPIInitCall>>
      instances;

  explicit PersistentMPIInitCall(llvm::CallBase *init_call);

  // default destructor

public:
  // first de do analysis than replacement
  // analysis would break if we first replace one call and than later try to
  // analyze it again in a different conflict we can also introduce a way of
  // propagating analysis results if a and b are conflict free b and a are as
  // well
  // void perform_analysis();
  void perform_replacement();

private:
  void populate_conflicting_calls();

  bool statically_proven_safe();
  void perform_statically_proven_safe_replacement();

  llvm::Value *
  get_conflict_result(std::shared_ptr<PersistentMPIInitCall> other);
  // true if conflict is not important and matching can be skipped
  // false if conflict is present and matching cannot be skipped
  llvm::Value *
  compute_conflict_result(std::shared_ptr<PersistentMPIInitCall> other);

  llvm::CallBase *init_call;
  // bool analyzed = false;
  bool replaced = false;
  llvm::Value *tag;
  llvm::Value *src;
  llvm::Value *comm;

  std::vector<std::shared_ptr<PersistentMPIInitCall>> conflicting_calls = {};
  std::map<std::shared_ptr<PersistentMPIInitCall>, llvm::Value *>
      conflict_results = {};

  // std::vector<std::tuple<llvm::Value *,llvm::Value *,llvm::Value *>>
  // conflicting_envelopes = {};
  llvm::Value *get_tag() { return tag; }
  llvm::Value *get_src() { return src; }
  llvm::Value *get_communicator() { return comm; }
};

bool check_mpi_recv_conflicts(llvm::CallBase *send_init_call);

bool check_mpi_send_conflicts(llvm::CallBase *recv_init_call);

// llvm::Value *get_communicator(llvm::CallBase *mpi_call);
// llvm::Value *get_src(llvm::CallBase *mpi_call, bool is_send);
// llvm::Value *get_tag(llvm::CallBase *mpi_call, bool is_send);

#endif /* MACH_CONFLICT_DETECTION_H_ */
