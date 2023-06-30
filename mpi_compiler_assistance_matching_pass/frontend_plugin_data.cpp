//
// Created by tim on 29.06.23.
//

#include "frontend_plugin_data.h"
#include "json.hpp"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include <cassert>
#include <llvm/Support/MemoryBuffer.h>
#include <vector>

using namespace llvm;

FrontendPluginData *FrontendPluginData::instance = nullptr;

FrontendPluginData::FrontendPluginData(llvm::Module &M) {
  // read json
  auto file = MemoryBuffer::getFileOrSTDIN("plugin_data.json");
  auto file_content = file.get()->getBuffer();
  auto plugin_data = nlohmann::json::parse(file_content);

  std::unordered_map<std::string, FunctionCallMetadata *> location_to_metadata;

  // parse json to struct
  for (const auto &functionCallJson : plugin_data["FunctionCalls"]) {
    FunctionCallMetadata functionCall;
    functionCall.functionName = functionCallJson["Function"];
    functionCall.sourceLocation = functionCallJson["SourceLocation"];
    functionCall.id = functionCallJson["id"];
    for (const auto &conflict : functionCallJson["Conflicts"]) {
      functionCall.conflicts.push_back(conflict);
    }
    functionCall.call = nullptr;
    functionCalls.push_back(std::move(functionCall));
    // get the address
    auto this_call = &functionCalls.back();

    if (location_to_metadata.find(this_call->sourceLocation) ==
        location_to_metadata.end()) {
      location_to_metadata[this_call->sourceLocation] = this_call;
    } else {
      assert(false && "Multiple metadata entries for same call found!");
    }
  }

  // find corresponding IR instructions

  for (auto F = M.begin(); F != M.end(); ++F) {
    for (inst_iterator I = inst_begin(*F), E = inst_end(*F); I != E; ++I) {
      if (auto *call = dyn_cast<CallBase>(&*I)) {
        if (auto debug_loc = call->getDebugLoc()) {
          std::string as_str;
          raw_string_ostream ss(as_str);
          debug_loc.print(ss);

          if (location_to_metadata.find(as_str) != location_to_metadata.end()) {
            // errs() << "FOUND METADATA FOR\n";
            // call->dump();
            if (call_to_metadata_map.find(call) == call_to_metadata_map.end()) {
              call_to_metadata_map[call] = location_to_metadata[as_str];
              call_to_metadata_map[call]->call = call;
            } else {
              assert(false && "Multiple calls found for same metadata entry");
            }
          }

        } else {
          // errs() << "NO DEBUG LOC\n";
          // call->dump();
        }
      }
    }
  }

  // read the adjacency matrix from json
  orderMatrix = plugin_data["Metadata"]["OrderMatrix"];

  unsigned int num_func_calls = plugin_data["Metadata"]["NumFunctionCalls"];
  // everything should have been read in correct order
  assert(functionCalls.size() == num_func_calls);
  assert(orderMatrix.size() == num_func_calls);
  for (unsigned int i = 0; i < functionCalls.size(); ++i) {
    assert(functionCalls[i].id == i);
    assert(orderMatrix[i].size() == num_func_calls);
  }
}

std::vector<llvm::CallBase *>
FrontendPluginData::get_possibly_conflicting_calls(llvm::CallBase *orig_call) {
  std::vector<llvm::CallBase *> result;

  // if frontend plugin does not know the call something went wrong
  assert(call_to_metadata_map.find(orig_call) != call_to_metadata_map.end());

  auto metadata = call_to_metadata_map[orig_call];

  // TODO one could initialize this vec at the time of reading in the plugin
  // result data
  for (auto i : metadata->conflicts) {
    result.push_back(functionCalls[i].call);
  }

  return result;
}

llvm::CallBase *FrontendPluginData::get_first_known_conflicting_call(
    llvm::CallBase *orig_call) {
  // TODO IMPLEMENT
  return nullptr;
}
