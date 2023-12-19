//
// Created by tim on 19.12.23.
//
#ifndef MPI_ASSERTION_CHECKING_PRECOMPUTE_INSERTION_H
#define MPI_ASSERTION_CHECKING_PRECOMPUTE_INSERTION_H

#include "precalculation.h"
#include <llvm/IR/Module.h>

void insert_precomputation(
    llvm::Module &M, const PrecalculationAnalysis &precompute_analyis_result);

#endif // MPI_ASSERTION_CHECKING_PRECOMPUTE_INSERTION_H
