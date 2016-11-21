//===--------------- CFGIndirectionCount.cpp - Annotating CFG Indirection--===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file ForcedLoopUnroll.cpp
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
//  This loop pass annotates mandatory CFG indirection count of all loads.
//
//===----------------------------------------------------------------------===//
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/IR/Dominators.h"

#include <fstream>
#include <sys/stat.h>

#include "Util/Annotation/MetadataInfo.h"
#include "Util/Analysis/LoopDependency.h"

#define DEBUG_TYPE "CFGIndirectionCount"

using namespace llvm;
using namespace std;
using namespace util;

static cl::opt<std::string>
    LoopName("loop-name",
             cl::desc("The keyword identifying the loop header to annotate"),
             cl::value_desc("name"));

namespace {
struct CFGIndirectionCount : public LoopPass {
  static char ID;

  CFGIndirectionCount() : LoopPass(ID) {}

public:
  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AAResultsWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<AssumptionCacheTracker>();
    AU.addRequired<TargetLibraryInfoWrapperPass>();
  }

  virtual bool runOnLoop(Loop *L, LPPassManager &LPM);
};
}

static void markIndirCount(Instruction *I, set<Instruction*> CFGDeps) {
  int LoadCount = count_if(CFGDeps.begin(), CFGDeps.end(), [](Instruction *DepI) {return isa<LoadInst>(DepI);});
  AttachMetadata(&(*I), "CFGIndir", to_string(LoadCount));
}

bool CFGIndirectionCount::runOnLoop(Loop *L, LPPassManager &LPM) {
  if (L->getHeader()->getName().find(LoopName) == string::npos) {
    return false;
  }

  // Check if it's a prologue loop. If so, it doesn't make sense to unroll
  if (L->getHeader()->getName().find(".prol") != string::npos) {
    return false;
  }

  // We need to manually construct BasicAA directly in order to disable
  // its use of other function analyses.
  BasicAAResult BAR(createLegacyPMBasicAAResult(*this, *(L->getHeader()->getParent())));

  // Construct our own AA results for this function. We do this manually to
  // work around the limitations of the legacy pass manager.
  AAResults AAR(createLegacyPMAAResults(*this, *(L->getHeader()->getParent()), BAR));
  AliasAnalysis *AA = &AAR;

  LoopInfo *LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

  for (Loop::block_iterator B = L->block_begin(), BE = L->block_end();
       B != BE; ++B) {
    for (BasicBlock::iterator I = (*B)->begin(), IE = (*B)->end(); I != IE;
	 ++I) {
      if (isa<LoadInst>(&*I)) {
	set<Instruction*> CFGDeps;
	getControlDeps(AA, LI, &*I, CFGDeps);
	markIndirCount(&*I, CFGDeps);
      }
    }
  }

  set<Instruction*> Deps;
}

char CFGIndirectionCount::ID = 0;
static RegisterPass<CFGIndirectionCount> X("annotate-cfg-indir",
					   "CFGIndirectionCount_pass",
					   true /* does not change CFG */,
					   false /* not an analysis pass */);
