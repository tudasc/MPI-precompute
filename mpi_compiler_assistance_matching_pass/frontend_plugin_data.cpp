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

  std::unordered_map<std::string, FunctionCallMetadata> location_to_metadata;

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
    if (location_to_metadata.find(functionCall.sourceLocation) ==
        location_to_metadata.end()) {
      location_to_metadata[functionCall.sourceLocation] =
          std::move(functionCall);
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
            errs() << "FOUND METADATA FOR\n";
            call->dump();
            if (functionCalls.find(call) == functionCalls.end()) {
              functionCalls[call] = std::move(location_to_metadata[as_str]);
              functionCalls[call].call = call;
            } else {
              assert(false && "Multiple calls found for same metadata entry");
            }
          }

        } else {
          errs() << "NO DEBUG LOC\n";
          call->dump();
        }
      }
    }
  }
}
