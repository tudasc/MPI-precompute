#ifndef LOOPOPTIMIZE_H
#define LOOPOPTIMIZE_H
#include <llvm/IR/Module.h>

void Optimize_loops(llvm::Module &M);
#endif // LOOPOPTIMIZE_H
