//===--------------- FindInstructions.cpp - Finding load insts-------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file FindInstructions.cpp
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// This file is a helper class containing functionality to find instructions
// to hoist.
//
//===----------------------------------------------------------------------===//
#include "FindInstructions.h"

#include "Util/DAE/DAEUtils.h"
#include "../../Utils/LongLatency.cpp"
#include "Util/Analysis/LoopCarriedDependencyAnalysis.h"
#include "LCDHandler.h"

void filterLoadsOnInterferingDeps(AliasAnalysis *AA, LoopInfo *LI, list<LoadInst *> &Loads,
                                  list<LoadInst *> &Hoistable, Function &F);
void filterLoadsOnIndir(AliasAnalysis *AA, LoopInfo *LI, list<LoadInst *> &LoadList, list<LoadInst *> &IndirList,
                        unsigned int IndirThresh);


// Overwrite to only pick delinquent loads
void findRelevantLoads(Function &F, list<LoadInst *> &LoadList, bool HoistDelinquent) {
  if (HoistDelinquent) {
    findDelinquentLoads(F, LoadList);
  } else {
    findLoads(F, LoadList);
  }
}

void filterLoadsOnInterferingDeps(AliasAnalysis *AA, LoopInfo *LI, list<LoadInst *> &Loads,
                                            list<LoadInst *> &Hoistable, Function &F) {
  // Hoistable, if CFG to this block doesn't require global stores / calls
  for (auto L = Loads.begin(), LE = Loads.end(); L != LE; ++L) {
    // this loads immediate deps
    set<Instruction *> Deps;

    // this laads populated deps in followDeps
    set<Instruction *> DepSet;
    getRequirementsInIteration(AA, LI, *L, Deps);
    if (followDeps(AA, Deps, DepSet)) {
      Hoistable.push_back(*L);
    }
  }
}

void filterLoadsOnIndir(AliasAnalysis *AA, LoopInfo *LI, list<LoadInst *> &LoadList, list<LoadInst *> &IndirList,
                        unsigned int IndirThresh) {
  for (list<LoadInst *>::iterator I = LoadList.begin(), E = LoadList.end(); I != E; ++I) {
    set<Instruction *> Deps;
    getDeps(AA, LI, *I, Deps);
    int DataIndirCount = count_if(Deps.begin(), Deps.end(),
                                  [&](Instruction *DepI){return isa<LoadInst>(DepI) && LI->getLoopFor(DepI->getParent());});
    bool UnderDataThreshold = DataIndirCount <= IndirThresh;
    bool UnderCFGThreshold = !InstrhasMetadataKind(*I, "CFGIndir") ||
        stoi(getInstructionMD(*I, "CFGIndir")) <= IndirThresh;

    if (UnderDataThreshold && UnderCFGThreshold) {
      IndirList.push_back(*I);
    }
    // else: hits indir threshold
  }
}

void findAccessInsts(AliasAnalysis *AA, LoopInfo *LI, Function &fun, list<LoadInst *> &toHoist, bool HoistDelinquent,
                     unsigned int IndirThresh) {
  list<LoadInst *> LoadList, VisibleList, IndirLoads;

  unsigned int BadDeps, Indir;

  // Find all existing load instructions
  findRelevantLoads(fun, LoadList, HoistDelinquent);

  findVisibleLoads(LoadList, VisibleList);

  // Filter on the number of allowed indirections to hoist
  filterLoadsOnIndir(AA, LI, VisibleList, IndirLoads, IndirThresh);
  Indir = VisibleList.size() - IndirLoads.size();

  anotateStores(AA, fun, IndirLoads);

  // Hoistable depending on terminator instructions
  filterLoadsOnInterferingDeps(AA, LI, IndirLoads, toHoist, fun);

  BadDeps = IndirLoads.size() - toHoist.size();

  errs() << "(BadDeps: " << BadDeps << ", Indir: " << Indir << ")\n";
}
