/*
 Copyright 2020 Tim Jammer

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

#include "mpi_functions.h"

#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Pass.h"

#include "analysis_results.h"

using namespace llvm;

RequiredAnalysisResults::RequiredAnalysisResults(
    llvm::ModuleAnalysisManager &MAM, llvm::Module &M) {

  assert(mpi_func != nullptr &&
         "The search for MPI functions should be made first");

  FAM = &MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

  // first function of M
  TLI = &FAM->getResult<TargetLibraryAnalysis>(*M.begin());
  MSI = &MAM.getResult<ModuleSummaryIndexAnalysis>(M);
}

llvm::AAResults *RequiredAnalysisResults::getAAResults(llvm::Function &f) {

  return &FAM->getResult<AAManager>(f);
}

llvm::LoopInfo *RequiredAnalysisResults::getLoopInfo(llvm::Function &f) {

  return &FAM->getResult<LoopAnalysis>(f);
}
llvm::ScalarEvolution *RequiredAnalysisResults::getSE(llvm::Function &f) {

  return &FAM->getResult<ScalarEvolutionAnalysis>(f);
}

llvm::DominatorTree *RequiredAnalysisResults::getDomTree(llvm::Function &f) {

  return &FAM->getResult<DominatorTreeAnalysis>(f);
}

llvm::TargetLibraryInfo *RequiredAnalysisResults::getTLI() { return TLI; }

llvm::PostDominatorTree *RequiredAnalysisResults::getPostDomTree(Function &f) {
  return &FAM->getResult<PostDominatorTreeAnalysis>(f);
}
