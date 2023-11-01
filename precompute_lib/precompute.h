
#ifndef PRECOMPUTE_H
#define PRECOMPUTE_H

#ifdef __cplusplus
extern "C" {
#endif

// initialization of precompute library
// call before the precomputation
void init_precompute_lib();

// TODO multi-threading is not supported
// TODO template for possible types
#define TYPE int

// value-ID if an intger to be used if one registeres multiple "types" of data
void register_precomputed_value(int value_id, TYPE value);
unsigned long get_num_precomputed_values(int value_id);
TYPE get_precomputed_value(int value_id, unsigned long idx);

// needed to allocate memory, so that the control-flow can abort when all values
// are precomputed finish_precomputation will free all memory allocated with
// this function
void *allocate_memory_in_precompute(unsigned long size);

// finish the precomputation: frees all ressources but the precompute results
void finish_precomputation();

// free everything
void free_precomputed_values();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PRECOMPUTE_H
