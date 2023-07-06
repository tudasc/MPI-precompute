
#ifndef MPI_ASSERTION_CHECKING_FRONTEND_PLUGIN_DATA_H
#define MPI_ASSERTION_CHECKING_FRONTEND_PLUGIN_DATA_H

#include "json.hpp"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include <cassert>
#include <unordered_map>
#include <vector>

enum CommunicationOrder
{
    BeforeInLoop = -2,
    Before = -1,
    Unknown = 0,
    After = 1,
    AfterInLoop = 2,
    UnknownInLoop = 3,
};

struct FunctionCallMetadata {
  std::vector<unsigned int> conflicts;
  std::string functionName;
  std::string sourceLocation;
  unsigned int id;
  llvm::CallBase *call;
};

class FrontendPluginData {

public:
  // singelton-like
  static FrontendPluginData *get_instance() {
    assert(instance != nullptr);
    return instance;
  }

  static FrontendPluginData *create_instance(llvm::Module &M) {
    if (instance == nullptr)
      instance = new FrontendPluginData(M);
    return instance;
  }

  static void delete_instance() {
    if (instance != nullptr)
      delete instance;
    instance = nullptr;
  }

  // singelton pattern: singeltons are NOT copy-able or assignable
  FrontendPluginData(FrontendPluginData &other) = delete;

  void operator=(const FrontendPluginData &) = delete;

private:
  static FrontendPluginData *instance;
  FrontendPluginData(llvm::Module &M);
  ~FrontendPluginData();

public:
  std::vector<llvm::CallBase *>
  get_possibly_conflicting_calls(llvm::CallBase *orig_call);
  llvm::CallBase *get_first_known_conflicting_call(llvm::CallBase *orig_call);

private:
  std::vector<FunctionCallMetadata> functionCalls;
  std::unordered_map<llvm::CallBase *, FunctionCallMetadata *>
      call_to_metadata_map;
  std::vector<std::vector<int>> orderMatrix;
};

#endif // MPI_ASSERTION_CHECKING_FRONTEND_PLUGIN_DATA_H
