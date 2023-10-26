#include "precompute.h"

#include "cassert"
#include "iostream"
#include "map"
#include "vector"

#define PRINT_REGISTERED_VALUES

#define MEASURE_PRECOMPUTE_TIME

#ifdef MEASURE_PRECOMPUTE_TIME
#include <chrono>
#endif

// is initialized, in precompute or in query phase
enum status {
  UNINITIALIZED,
  IN_PRECOMPUTE,
  READY_FOR_QUERY,
  FREED,
};

enum status status = UNINITIALIZED;

std::map<int, std::vector<TYPE>> precomputed_vals;

std::vector<void *> allocated_ptrs;

#ifdef MEASURE_PRECOMPUTE_TIME
std::chrono::steady_clock::time_point begin;
#endif

// initialization of precompute library
// call before the precomputation
void init_precompute_lib() {
  assert(status == UNINITIALIZED);
  status = IN_PRECOMPUTE;
  precomputed_vals = {};
  allocated_ptrs = {};
#ifdef PRINT_REGISTERED_VALUES
  std::cout << "Begin Precompute\n";
#endif
#ifdef MEASURE_PRECOMPUTE_TIME
  begin = std::chrono::steady_clock::now();
#endif
}

void register_precomputed_value(int value_id, TYPE value) {
  assert(status == IN_PRECOMPUTE);

  auto pos = precomputed_vals.find(value_id);
  if (pos == precomputed_vals.end()) {
    precomputed_vals[value_id] = std::vector<TYPE>();
    pos = precomputed_vals.find(value_id);
  }
  assert(pos != precomputed_vals.end());
  pos->second.push_back(value);
#ifdef PRINT_REGISTERED_VALUES
  std::cout << "Register " << value << " (Type " << value_id << ")\n";
#endif
}

unsigned long get_num_precomputed_values(int value_id) {
  assert(status == READY_FOR_QUERY);
  auto pos = precomputed_vals.find(value_id);
  if (pos != precomputed_vals.end()) {
    return pos->second.size();
  } else {
    return 0;
  }
}

TYPE get_precomputed_value(int value_id, unsigned long idx) {
  assert(status == READY_FOR_QUERY);
  auto pos = precomputed_vals.find(value_id);
  assert(pos != precomputed_vals.end());
  assert(idx < pos->second.size());
  return pos->second[idx];
}

void *allocate_memory_in_precompute(unsigned long size) {
  assert(status == IN_PRECOMPUTE);
  void *new_ptr = calloc(size, 1);
  allocated_ptrs.push_back(new_ptr);
  return new_ptr;
}

void finish_precomputation() {
  assert(status == IN_PRECOMPUTE);
  status = READY_FOR_QUERY;
  for (void *ptr : allocated_ptrs) {
    free(ptr);
  }
  allocated_ptrs.clear();
#ifdef PRINT_REGISTERED_VALUES
  std::cout << "End Precompute\n";
#endif
#ifdef MEASURE_PRECOMPUTE_TIME
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  std::cout << "Precompute Time: "
            << std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                     begin)
                   .count()
            << " [µs]" << std::endl;
#endif
}

void free_precomputed_values() {
  precomputed_vals.clear();

  status = FREED;
}
