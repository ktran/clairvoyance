//===--------------- ForcedLoopUnroll.cpp - Loop Unroll Util
//---------------------===//
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
// This loop pass unrolls a certain loop specified by a keyword. It unrolls
// regardless of any unroll cost.
//
//===----------------------------------------------------------------------===//
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include <fstream>
#include <llvm/IR/Dominators.h>
#include <sys/stat.h>

#define DEBUG_TYPE "forceunroll"

STATISTIC(NumStatic, "Number of statically unrolled loops.");
STATISTIC(NumRuntime, "Number of runtime unrolled loops.");
STATISTIC(NumBranch, "Number of unrolled loops with conditions.");
STATISTIC(NumFailed, "Number of failed unrolled loops.");

using namespace llvm;
using namespace std;

static cl::opt<std::string>
    LoopName("loop-name",
             cl::desc("The keyword identifying the loop header to unroll"),
             cl::value_desc("name"));

static cl::opt<unsigned>
    UnrollCount("unroll", cl::desc("Max number of unrolled iterations"),
                cl::value_desc("unsigned"));

namespace {
struct ForcedLoopUnroll : public LoopPass {
  static char ID;

  ForcedLoopUnroll() : LoopPass(ID) {}

public:
  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AssumptionCacheTracker>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.addRequired<DominatorTreeWrapperPass>();
  }

  virtual bool runOnLoop(Loop *L, LPPassManager &LPM);
};
}

bool ForcedLoopUnroll::runOnLoop(Loop *L, LPPassManager &LPM) {
  if (L->getHeader()->getName().find(LoopName) == string::npos) {
    return false;
  }

  if (UnrollCount <= 1) {
    return false;
  }

  // Check if it's a prologue loop. If so, it doesn't make sense to unroll
  if (L->getHeader()->getName().find(".prol") != string::npos) {
    return false;
  }

  ScalarEvolution *SE = &getAnalysis<ScalarEvolutionWrapperPass>().getSE();
  unsigned Count = UnrollCount;
  unsigned TripCount = 0;
  unsigned TripMultiple = 1;

  //
  BasicBlock *ExitingBlock = L->getLoopLatch();
  if (!ExitingBlock || !L->isLoopExiting(ExitingBlock))
    ExitingBlock = L->getExitingBlock();

  // Find trip count and trip multiple
  if (ExitingBlock) {
    TripCount = SE->getSmallConstantTripCount(L, ExitingBlock);
    TripMultiple = SE->getSmallConstantTripMultiple(L, ExitingBlock);
  }

  if (TripCount != 0 && Count > TripCount)
    Count = TripCount;

  assert(Count > 0);
  assert(TripMultiple > 0);
  assert(TripCount == 0 || TripCount % TripMultiple == 0);

  LoopInfo *LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  AssumptionCache *AC =
      &getAnalysis<AssumptionCacheTracker>().getAssumptionCache(
          *L->getHeader()->getParent());
  DominatorTree *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();

  errs() << "Unrolling .. : " << L->getHeader()->getName() << "\n";
  bool AllowRuntimeUnroll = false;
  bool AllowExpensiveTripCount = true;
  bool PreserveLCSSA = mustPreserveAnalysisID(LCSSAID);
  bool UnrollSucceeded;

  // Unroll loop. Will unroll dynamically if trip count is not known (i.e.
  // TripCount = 0)
  UnrollSucceeded = UnrollLoop(L, Count, TripCount, AllowRuntimeUnroll,
                               AllowExpensiveTripCount, TripMultiple, LI, SE,
                               DT, AC, PreserveLCSSA);

  if (UnrollSucceeded) {
    ++NumStatic;
    errs() << "Succeeded with static unrolling\n";
  } else {
    AllowRuntimeUnroll = true;
    UnrollSucceeded = UnrollLoop(L, Count, TripCount, AllowRuntimeUnroll,
				 AllowExpensiveTripCount, TripMultiple, LI, SE,
				 DT, AC, PreserveLCSSA);

    if (UnrollSucceeded) {
      ++NumRuntime;
      errs() << "Succeeded with runtime unrolling\n";
    }
  }

  if (!UnrollSucceeded) {
    AllowRuntimeUnroll = false;
    UnrollSucceeded = UnrollLoop(L, Count, TripCount, AllowRuntimeUnroll,
                                 AllowExpensiveTripCount, TripMultiple, LI, SE,
                                 DT, AC, PreserveLCSSA);

    if (UnrollSucceeded) {
      ++NumBranch;
      errs() << "Succeeded with forced unrolling (including branches)\n";
    } else {
      ++NumFailed;
    }
  }

  return UnrollSucceeded;
}

char ForcedLoopUnroll::ID = 0;
static RegisterPass<ForcedLoopUnroll> X("single-loop-unroll",
                                        "LoopUnrollUtil_pass", true, true);
