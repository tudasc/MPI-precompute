#include "Openmp_region.h"

#include "openmp_runtime_functions.h"

#include <llvm/IR/Constants.h>

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

      if (is_thread_fork_call(call)) {
        assert(ompoutlined == call->getArgOperand(2));
        _fork_calls.push_back(call);
      } else if (call->getCalledFunction() &&
                 call->getCalledFunction() ==
                     get_omp_functions(*ompoutlined->getParent())
                         ->kmpc_omp_task_alloc) {
        _task_alloc_calls.push_back(call);
        _is_task = true;
        for (auto *sched_call : get_task_scheduling_calls(call)) {
          _task_sched_calls.push_back(sched_call);
        }

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
  if (_is_task) {
    get_shared_vars_in_task();
  } else {
    get_shared_vars_in_parallel();
  }

  // Look for a parallel for inside the microtask;
  // TODO add the other kmpc functions for parallel for pragamas
  auto call_instructions = get_instruction_in_function<CallInst>(_function);
  for (auto &call :
       call_instructions) { // if thre are indirect calls in parallel region
    if (call->getCalledFunction()) {
      if (call->getCalledFunction() &&
          call->getCalledFunction()->getName() == "__kmpc_for_static_init_4") {
        _parallel_for.init = call;
      } else if (call->getCalledFunction() &&
                 call->getCalledFunction()->getName() ==
                     "__kmpc_for_static_fini") {
        _parallel_for.fini = call;
      }

      // Look for a reduction inside the microtask;
      // TODO add the other kmpc functions for reduction pragmas

      else if (call->getCalledFunction()->getName() == "__kmpc_reduce_nowait") {
        _reduction.reduce = call;
      } else if (call->getCalledFunction()->getName() ==
                 "__kmpc_end_reduce_nowait") {
        _reduction.end_reduce = call;
      }
    }
  }
}

ParallelRegion::~ParallelRegion() {}

void ParallelRegion::get_shared_vars_in_parallel() {
  assert(not _is_task);
  for (auto &argument : _function->args()) {
    // The first shared variable has index 2
    if (argument.getArgNo() > 1) {
      if (argument.getType()->isPointerTy()) {
        // otherwise it is private
        _shared_variables.push_back(&argument);
      }
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
}

void sort_parallel_gep_indices(
    std::vector<std::pair<GetElementPtrInst *, Value *>> &geps) {
  // sort based on gep idx
  std::sort(geps.begin(), geps.end(), [](auto lhs, auto rhs) {
    GetElementPtrInst *gep_a = lhs.first;
    GetElementPtrInst *gep_b = rhs.first;
    auto *idx_b = gep_b->idx_begin();
    for (auto *idx_a = gep_a->idx_begin(); idx_a != gep_a->idx_end(); ++idx_a) {
      assert(idx_b != gep_b->idx_end());
      assert(isa<ConstantInt>(idx_a) && isa<ConstantInt>(idx_b) &&
             "Non constant access to task struct");
      if (cast<ConstantInt>(idx_a)->getZExtValue() <
          cast<ConstantInt>(idx_b)->getZExtValue()) {
        return true;
      } else if (cast<ConstantInt>(idx_a)->getZExtValue() >
                 cast<ConstantInt>(idx_b)->getZExtValue()) {
        return false;
      }
      // else equals

      ++idx_b;
    }
    // same

    assert(0 && "fail to determine order of parameters in openmp task");
  });
}
void ParallelRegion::get_shared_vars_in_task() {
  assert(_is_task);
  _function->dump();
  auto arg = _function->getArg(1);   // struct address
  LoadInst *load_parallel = nullptr; // load struct to shared vars
  for (auto u : arg->users()) {
    if (auto *load_inst = dyn_cast<LoadInst>(u)) {
      assert(load_parallel == nullptr && "Only one load of omp task struct");
      load_parallel = load_inst;
    }
  }
  if (not load_parallel) {
    _to_serial_map[arg] = {};
    for (auto *task_alloc : _task_alloc_calls) {
      _to_serial_map[arg].push_back(task_alloc);
      _to_parallel_map[task_alloc] = arg;
    }
    return; // nothing to do: no shared vars used
    // ensure that the struct aliases though
  }
  // collect shared variables
  std::vector<std::pair<GetElementPtrInst *, Value *>> parallel_geps;
  LoadInst *shared_var_0 = nullptr;
  for (auto *u : load_parallel->users()) {
    if (auto *gep_parallel = dyn_cast<GetElementPtrInst>(u)) {
      for (auto uu : gep_parallel->users()) {
        if (auto *ll_parallel = dyn_cast<LoadInst>(uu)) {
          parallel_geps.push_back(std::make_pair(gep_parallel, ll_parallel));
        }
      }
    } else if (auto *ll_parallel = dyn_cast<LoadInst>(u)) {
      // direct usage = gep 0
      assert(shared_var_0 == nullptr &&
             "not supported usage of omp task struct");
      shared_var_0 = ll_parallel;

    } else if (auto *cc = dyn_cast<CallBase>(u)) {
      if (!cc->getCalledFunction() && cc->getName().starts_with("__tsan")) {
        u->dump();
        assert(0 && "not supported usage of omp task struct");
      }
      // nothing to do
    } else {
      u->dump();
      assert(0 && "not supported usage of omp task struct");
    }
  }
  sort_parallel_gep_indices(parallel_geps);

  _to_serial_map[load_parallel] = {};
  _to_serial_map[arg] = {};
  if (shared_var_0) {
    _to_serial_map[shared_var_0] = {};
    if (shared_var_0->getType()->isPointerTy()) {
      // else it is private
      _shared_variables.push_back(shared_var_0);
    }
  }
  for (auto p : parallel_geps) {
    _to_serial_map[p.first] = {};
    _shared_variables.push_back(p.first);
    _to_serial_map[p.second] = {};
    if (p.second->getType()->isPointerTy()) {
      // else it is private
      _shared_variables.push_back(p.second);
    }
  }
  for (auto *task_alloc : _task_alloc_calls) {
    _to_serial_map[arg].push_back(task_alloc);
    _to_parallel_map[task_alloc] = arg;
    for (auto u : task_alloc->users()) {
      if (auto *load_serial = dyn_cast<LoadInst>(u)) {
        _to_serial_map[load_parallel].push_back(load_serial);
        _to_parallel_map[load_serial] = load_parallel;

        Value *shared_var_0_serial = nullptr;
        std::vector<std::pair<GetElementPtrInst *, Value *>> serial_geps;
        for (auto *uu : load_serial->users()) {
          uu->dump();
          if (auto *gep_serial = dyn_cast<GetElementPtrInst>(uu)) {
            for (auto uuu : gep_serial->users()) {
              if (auto *store_shared_var = dyn_cast<StoreInst>(uuu)) {
                serial_geps.push_back(std::make_pair(
                    gep_serial, store_shared_var->getValueOperand()));
              }
            }
          } else if (auto *store_serial = dyn_cast<StoreInst>(uu)) {
            // direct usage = gep 0
            assert(shared_var_0_serial == nullptr &&
                   "not supported usage of omp task struct");
            shared_var_0_serial = store_serial->getValueOperand();
          } else if (auto *ll = dyn_cast<LoadInst>(uu)) {
            // nothing to do, serial may read again
            // this happens with openmp if and conditional task creation

          } else if (auto *cc = dyn_cast<CallBase>(uu)) {
            if (!cc->getCalledFunction() &&
                cc->getName().starts_with("__tsan")) {
              u->dump();
              assert(0 && "not supported usage of omp task struct");
            }
            // nothing to do
          } else {
            u->dump();
            assert(0 && "not supported usage of omp task struct");
          }
        }
        // sort based on gep idxs
        sort_parallel_gep_indices(serial_geps);

        // map the values from serial and paralllel

        if (shared_var_0_serial || !serial_geps.empty()) {
          // else it is an inline execution of the task (task not created with
          // omp if)
          if (shared_var_0) {
            assert(shared_var_0_serial);
            _to_serial_map[shared_var_0].push_back(shared_var_0_serial);
            _to_parallel_map[shared_var_0_serial] = shared_var_0;
          }

          errs() << "serial:\n";
          for (auto p : serial_geps)
            p.first->dump();
          errs() << "parallel:\n";
          for (auto p : parallel_geps)
            p.first->dump();

          assert(serial_geps.size() == parallel_geps.size());

          for (unsigned long i = 0; i < serial_geps.size(); i++) {
            _to_serial_map[parallel_geps[i].first].push_back(
                serial_geps[i].first);
            _to_parallel_map[serial_geps[i].first] = parallel_geps[i].first;

            _to_serial_map[parallel_geps[i].second].push_back(
                serial_geps[i].second);
            _to_parallel_map[serial_geps[i].second] = parallel_geps[i].second;
          }
        }
      }
    }
  }
}

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
llvm::Value *ParallelRegion::get_value_in_parallel(llvm::Value *val) {

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
        return bb.getName().starts_with("omp.dispatch.cond.omp.dispatch.end");
      });

  if (it == _function->end()) {
    errs() << "Error analyzing the loops structure\n";
    return nullptr;
  } else {
    return &*it;
  }
}
