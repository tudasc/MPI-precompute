
#include "std_funcs.h"
#include "openmp_runtime_functions.h"

#include "analysis_results.h"
#include "precalculation_function_analysis.h"

#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Demangle/Demangle.h>

using namespace llvm;
// an interaction with std::cout can not except during precompute (any exception
// would be considered fatal anyway) therefore we do not need to analyze the
// interactions with std::cout
bool is_interaction_with_cout(llvm::CallBase *call) {
  if (call->isIndirectCall()) {
    return false; // may except
  }

  if (call->getCalledFunction()->getName() == "printf") {
    return true;
  }

  auto *cout = call->getModule()->getGlobalVariable("_ZSt4cout");
  if (cout) {
    // errs() << "check if interaction with cout:\n";
    // call->dump();

    if (call->arg_size() >= 2 && call->getArgOperand(0) == cout) {
      // assert(is_func_from_std(call->getCalledFunction()));
      // errs() << "TRUE: interaction with cout:\n";
      return true;
    }
    auto name = get_function_name(
        llvm::demangle(call->getCalledFunction()->getName().str()));
    if (name.find("operator<<") != std::string::npos) {
      // if multiple chained usages of operator << operator << will be used on
      // the result of first application of operator<<
      assert(call->arg_size() >= 2);
      if (auto *cc = dyn_cast<CallBase>(call->getArgOperand(0))) {
        // errs() << "Defer to other call: interaction with cout:\n";
        return is_interaction_with_cout(cc);
      }
      if (auto *phi = dyn_cast<PHINode>(call->getArgOperand(0))) {
        // std::all_of
        for (auto &incoming : phi->incoming_values()) {
          if (auto *cc = dyn_cast<CallBase>(&incoming)) {
            // errs() << "Defer to other call: interaction with cout:\n";
            if (not is_interaction_with_cout(cc)) {
              return false;
            }
          } else {
            return false;
          }
        }
        return true;
      }
    }
  }
  // errs() << "FALSE: no interaction with cout:\n";
  return false;
}

static std::vector<std::string> allowed_function_prefixes = {};
void initialize_allowed_function_prefixes() {
  allowed_function_prefixes = {// from std
                               "std::",
                               // internals of gnu implementation
                               "__gnu_cxx::"};

  // read settings from environment var
  if (const char *env_p =
          std::getenv("COMPILER_ASSISTED_MATCHING_ALLOW_EXTERNAL_FUNCTIONS")) {
    std::istringstream iss(env_p);
    std::string item;
    std::vector<std::string> elems;
    while (std::getline(iss, item, ':')) {
      if (item != "") {
        llvm::errs() << "Allow function namespace: " << item << "\n";
        allowed_function_prefixes.push_back(item + "::");
      }
    }
  }
}

void allow_function_prefixes_to_be_called_in_precompute(
    const std::vector<std::string> &prefixes_to_allow) {
  assert(allowed_function_prefixes.empty() && "it was already initialized");
  initialize_allowed_function_prefixes();
  std::copy(prefixes_to_allow.begin(), prefixes_to_allow.end(),
            std::back_inserter(allowed_function_prefixes));
}

llvm::Function *std_dummy_func = nullptr;

llvm::Function *get_std_dummy_func(llvm::Module *M) {
  if (!std_dummy_func) {
    std_dummy_func = Function::Create(
        FunctionType::get(Type::getVoidTy(M->getContext()), false),
        GlobalValue::InternalLinkage, "std_dummy_func");
  }

  return std_dummy_func;
}

bool is_func_from_std(llvm::Function *func) {
  assert(func);
  if (allowed_function_prefixes.empty()) {
    // read in env variable one time
    initialize_allowed_function_prefixes();
  }

  // openmp
  if (is_omp_function(func)) {
    return true;
  }
  if (func == get_std_dummy_func(func->getParent())) {
    return true;
  }

  // C API
  llvm::LibFunc lib_func;
  bool in_lib = analysis_results->getTLI()->getLibFunc(*func, lib_func);
  if (in_lib) {
    return true;
  }

  auto demangled_fname =
      get_function_name(llvm::demangle(func->getName().str()));

  // errs() << "Test if in std:\n" << func->getName() <<demangled_fname <<
  // "\n";

  for (auto prefix : allowed_function_prefixes) {
    if (demangled_fname.rfind(prefix, 0) == 0) {
      return true;
    }
  }

  if (func->getName() == "llvm.va_start" || func->getName() == "llvm.va_end") {
    return true;
  }

  // more like a stack ptr than a function call
  if (func->getName() == "__errno_location") {
    return true;
  }

  if (func->getName() == "__cxa_throw") {
    return true;
  }

  if (func->getName() == "__cxa_allocate_exception") {
    // TODO is allocator
    return true;
  }
  if (func->getName() == "__cxa_free_exception") {
    // TODO is free
    return true;
  }
  if (func->getName() == "__cxa_begin_catch") {
    // TODO is free
    return true;
  }

  // openmp lib funcs
  if (func->getName() == "omp_get_max_threads") {
    return true;
  }
  if (func->getName() == "omp_get_thread_num") {
    return true;
  }

  // TODO why it is not in TLI info??
  if (func->getName() == "rand" || func->getName() == "rand_r" ||
      func->getName() == "srand" ||
      // calling rand in precompute is actually "safe",
      // as one should usa a random seed anyway it doesn't matter if we call
      // it in precompute
      func->getName() == "getrusage" || func->getName() == "time" ||
      func->getName() == "localtime" || func->getName() == "clock_gettime" ||
      func->getName() == "isspace" || func->getName() == "isalpha" ||
      func->getName() == "isalnum" || func->getName() == "isdigit" ||

      // from gnu
      func->getName() == "__getdelim") {

    return true;
  }

  // if std=c99 is supplied to the compiler
  if (func->getName().starts_with("__isoc99_")) {
    return true;
  }

  return false;
}