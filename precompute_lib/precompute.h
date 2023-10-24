
#ifndef PRECOMPUTE_H
#define PRECOMPUTE_H

#ifdef __cplusplus
extern "C" {
#endif

// initialization of precompute library
// call before the precomputation
void init_precompute_lib();

// TODO template for possible types
#define TYPE int

// value-ID if an intger to be used if one registeres multiple "types" of data
void register_precomputed_value(int value_id, TYPE value);
unsigned long get_num_precomputed_values(int value_id);
TYPE get_precomputed_value(int value_id, unsigned long idx);

// finish the precomputation: frees all ressources but the precompute results
void finish_precomputation();

// free everything
void free_precomputed_values();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PRECOMPUTE_H
