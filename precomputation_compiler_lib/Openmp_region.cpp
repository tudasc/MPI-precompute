#include "Openmp_region.h"

#include "openmp_runtime_functions.h"

using namespace llvm;

/**
 * Searches the Function for all uses of instructions of type T and returns them
 *in a vector
 **/
template <class T>
std::vector<T *> get_instruction_in_function(llvm::Function *func) {
  std::vector<T *> instructions;
  for (auto &B : *func) {
    for (auto &I : B) {
      if (auto *inst = llvm::dyn_cast<T>(&I)) {
        instructions.push_back(inst);
      }
    }
  }
  return instructions;
}

// combine std::find_if and std::none_of to write stl-like find_if_exactly_one
template <class InputIt, class UnaryPredicate>
InputIt find_if_exactly_one(InputIt first, InputIt last, UnaryPredicate p) {
  auto it = std::find_if(first, last, p);
  if ((it != last) && std::none_of(std::next(it), last, p))
    return it;
  else
    return last;
}

ParallelRegion::ParallelRegion(Function *ompoutlined) {

  _function = ompoutlined;

  for (auto u : ompoutlined->users()) {
    if (auto *call = dyn_cast<CallBase>(u)) {

      if (is_omp_fork_call(call)) {
        assert(ompoutlined == call->getArgOperand(2));
        _fork_calls.push_back(call);
      } else {
        // direct call to ompoutlined == with one thread only
        assert(call->getCalledFunction() == ompoutlined);
      }

    } else {
      u->dump();
      assert(0 && "Not implemented user of ompoutlined");
    }
  }

  _parallel_for.init = nullptr;
  _parallel_for.fini = nullptr;
  _reduction.reduce = nullptr;
  _reduction.end_reduce = nullptr;

  // Collect the shared variables
  for (auto &argument : _function->args()) {
    // The first shared variable has index 2
    if (argument.getArgNo() > 1) {
      //_shared_variables.push_back(new SharedVariable(&argument, _function));
      Value *in_parallel = &argument;
      // first shared arg
      int start_args_at_fork = 3;
      // first two args of ompoutlined are not interesting
      start_args_at_fork = start_args_at_fork - 2;

      _to_serial_map[in_parallel] = {};
      for (auto *c : _fork_calls) {
        Value *in_serial =
            c->getArgOperand(start_args_at_fork + argument.getArgNo());
        _to_serial_map[in_parallel].push_back(in_serial);
        _to_parallel_map[in_serial] = in_parallel;
      }
    }
  }

  // Look for a parallel for inside the microtask;
  // TODO add the other kmpc functions for parallel for pragamas
  auto call_instructions = get_instruction_in_function<CallInst>(_function);
  for (auto &call : call_instructions) {
    if (call->getCalledFunction()->getName().equals(
            "__kmpc_for_static_init_4")) {
      _parallel_for.init = call;
    } else if (call->getCalledFunction()->getName().equals(
                   "__kmpc_for_static_fini")) {
      _parallel_for.fini = call;
    }
  }

  // Look for a reduction inside the microtask;
  // TODO add the other kmpc functions for reduction pragmas
  for (auto &call : call_instructions) {
    if (call->getCalledFunction()->getName().equals("__kmpc_reduce_nowait")) {
      _reduction.reduce = call;
    } else if (call->getCalledFunction()->getName().equals(
                   "__kmpc_end_reduce_nowait")) {
      _reduction.end_reduce = call;
    }
  }
}

ParallelRegion::~ParallelRegion() {}

std::vector<CallBase *> ParallelRegion::get_fork_calls() { return _fork_calls; }

Function *ParallelRegion::get_function() { return _function; }

ParallelForData *ParallelRegion::get_parallel_for() {
  if (_parallel_for.init != nullptr && _parallel_for.fini != nullptr) {
    return &_parallel_for;
  } else {
    return nullptr;
  }
}

ReductionData *ParallelRegion::get_reduction() {
  if (_reduction.reduce != nullptr && _reduction.end_reduce != nullptr) {
    return &_reduction;
  } else {
    return nullptr;
  }
}

// gets the value that corresponds to the given value from main
llvm::Argument *ParallelRegion::get_value_in_parallel(llvm::Value *val) {

  return _to_parallel_map.count(val) > 0
             ? cast<Argument>(_to_parallel_map.at(val))
             : nullptr;
}
// get the value that corresponds to the given value in microtask
std::vector<llvm::Value *>
ParallelRegion::get_value_in_serial(llvm::Value *val) {
  return _to_serial_map.count(val) > 0 ? _to_serial_map.at(val)
                                       : std::vector<llvm::Value *>{};
}

BasicBlock *ParallelRegion::find_loop_end_block() {

  if (this->get_parallel_for() == nullptr) {
    return nullptr;
  }

  auto it = find_if_exactly_one(
      _function->begin(), _function->end(), [](BasicBlock &bb) {
        return bb.getName().startswith("omp.dispatch.cond.omp.dispatch.end");
      });

  if (it == _function->end()) {
    errs() << "Error analyzing the loops structure\n";
    return nullptr;
  } else {
    return &*it;
  }
}
