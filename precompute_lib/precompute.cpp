#include "precompute.h"

#include "cassert"
#include "iostream"
#include "map"
#include "vector"

#define PRINT_REGISTERED_VALUES

// is initialized, in precompute or in query phase
enum status {
  UNINITIALIZED,
  IN_PRECOMPUTE,
  READY_FOR_QUERY,
  FREED,
};

enum status status = UNINITIALIZED;

std::map<int, std::vector<TYPE>> precomputed_vals;

// initialization of precompute library
// call before the precomputation
void init_precompute_lib() {
  assert(status == UNINITIALIZED);
  status = IN_PRECOMPUTE;
  precomputed_vals = {};
#ifdef PRINT_REGISTERED_VALUES
  std::cout << "Begin Precompute\n";
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

void finish_precomputation() {
  assert(status == IN_PRECOMPUTE);
  status = READY_FOR_QUERY;
#ifdef PRINT_REGISTERED_VALUES
  std::cout << "End Precompute\n";
#endif
}

void free_precomputed_values() {
  precomputed_vals.clear();

  status = FREED;
}
