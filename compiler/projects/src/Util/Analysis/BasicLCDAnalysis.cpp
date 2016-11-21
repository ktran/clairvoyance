//===- LoopCarriedDependencyAnalysis - LCD Analysis based on LLVM DA ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file BasicLCDAnalysis.cpp
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// This file defines the loop carried dependency analysis based on information
// gained from LLVM's dependence analysis.
//
//===----------------------------------------------------------------------===//

#include "Util/Analysis/LoopCarriedDependencyAnalysis.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <queue>

#define DEBUG_TYPE "-lcd-analysis"

using namespace llvm;
using namespace util;

namespace util {
class BasicLCDAnalysis : public LoopCarriedDependencyAnalysis {
public:
  BasicLCDAnalysis(DependenceAnalysis *DepA, LoopInfo *LoopI)
      : DA(DepA), LI(LoopI) {}

  ~BasicLCDAnalysis() {
    // std::map<Loop, SmallVectorImpl<Instruction*>*>::iterator M, ME;
    // for (M = LoopToMemInst.begin(), ME = LoopToMemInst.end();
    // 	   M != ME; ++M) {
    // 	delete M->second;
    // }
  }

  const LCDResult checkLCD(Instruction *I, const Loop *L);
  bool getLCDDistance(Instruction *I, const Loop *L, long int &Distance);
  void setup(Function &F);

private:
  DependenceAnalysis *DA;
  LoopInfo *LI;

  typedef std::pair<const Loop *, Instruction *> DependenceKey;
  typedef SmallVector<std::unique_ptr<Dependence *>, 16> Dependences;
  typedef std::map<DependenceKey,
                   SmallVectorImpl<std::unique_ptr<Dependence>> *>
      LCDCacheTy;
  LCDCacheTy LCDCache;

  typedef SmallVector<Instruction *, 16> InstVectorTy;
  std::map<const Loop *, SmallVectorImpl<Instruction *> *> LoopToMemInst;

  SmallVectorImpl<std::unique_ptr<Dependence>> *
  getLCDInfo(Instruction *I, SmallVectorImpl<Instruction *> *MemInst,
             const Loop *L);

  SmallVectorImpl<std::unique_ptr<Dependence>> *getDependencies(Instruction *I,
                                                                const Loop *L);
};
}

void BasicLCDAnalysis::setup(Function &F) {
  std::queue<const Loop *> Loops;

  for (LoopInfo::iterator L = LI->begin(), LE = LI->end(); L != LE; ++L) {
    Loops.push(*L);
  }

  while (!Loops.empty()) {
    const Loop *LP = Loops.front();
    Loops.pop();

    InstVectorTy *MemInst = new InstVectorTy();
    collectMemInst(*LP, *MemInst);
    LoopToMemInst[LP] = MemInst;

    std::vector<Loop *> subLoops = LP->getSubLoops();
    for (auto SL : subLoops) {
      Loops.push(SL);
    }
  }
}

bool BasicLCDAnalysis::getLCDDistance(Instruction *I, const Loop *L,
                                      long int &Distance) {
  SmallVectorImpl<std::unique_ptr<Dependence>> *Deps = getDependencies(I, L);
  Distance = LONG_MAX;
  if (Deps->size() == 0) {
    // No dependencies existing
    return true;
  }

  bool ValidDistance = true;
  unsigned LoopLevel = L->getLoopDepth();
  SmallVectorImpl<std::unique_ptr<Dependence>>::iterator D = Deps->begin();

  while (D != Deps->end()) {
    unsigned Levels = (*D)->getLevels();
    if (Levels >= LoopLevel) {

      if ((*D)->isConfused()) {
        ValidDistance = false;
        break;
      }

      const SCEV *Dist = (*D)->getDistance(LoopLevel);
      const SCEVConstant *SCEVConst = dyn_cast_or_null<SCEVConstant>(Dist);
      if (SCEVConst) {
        Distance = std::min(SCEVConst->getValue()->getSExtValue(), Distance);
      } else {
        ValidDistance = false;
        break;
      }
    }

    ++D;
  }

  return ValidDistance;
}

SmallVectorImpl<std::unique_ptr<Dependence>> *
BasicLCDAnalysis::getDependencies(Instruction *I, const Loop *L) {
  DependenceKey Key = std::make_pair(L, I);
  LCDCacheTy::iterator DepI = LCDCache.find(Key);

  SmallVectorImpl<std::unique_ptr<Dependence>> *Deps;
  if (DepI == LCDCache.end()) {
    SmallVectorImpl<Instruction *> *MemInst = LoopToMemInst[L];
    Deps = getLCDInfo(I, MemInst, L);
  } else {
    Deps = DepI->second;
  }

  return Deps;
}

const LCDResult BasicLCDAnalysis::checkLCD(Instruction *I, const Loop *L) {
  if (!isa<LoadInst>(I) && !isa<StoreInst>(I)) {
    return LCDResult::NoLCD;
  }

  SmallVectorImpl<std::unique_ptr<Dependence>> *Deps = getDependencies(I, L);
  unsigned LoopLevel = L->getLoopDepth();
  SmallVectorImpl<std::unique_ptr<Dependence>>::iterator D = Deps->begin();
  LCDResult LCDRes = LCDResult::NoLCD;

  while (D != Deps->end() && LCDRes != LCDResult::MustLCD) {
    unsigned Levels = (*D)->getLevels();
    if (Levels < LoopLevel) {
      ++D;
      continue;
    }

    if ((*D)->isConfused()) {
      LCDRes = LCDResult::MayLCD;
      ++D;
      continue;
    }

    unsigned Dir = (*D)->getDirection(LoopLevel);
    if (Dir == Dependence::DVEntry::EQ) {
      LCDRes = LCDResult::NoLCD;
    } else if (isa<LoadInst>(I) && Dir == Dependence::DVEntry::LT) {
      LCDRes = LCDResult::NoLCD;
    } else if ((*D)->isOutput() || (*D)->isInput()) {
      LCDRes = LCDResult::NoLCD;
    } else {
      LCDRes = LCDResult::MustLCD;
    }
    ++D;
  }

  return LCDRes;
}

SmallVectorImpl<std::unique_ptr<Dependence>> *BasicLCDAnalysis::getLCDInfo(
    Instruction *I, SmallVectorImpl<Instruction *> *MemInst, const Loop *L) {

  LCDResult LCDRes = LCDResult::NoLCD;

  SmallVectorImpl<std::unique_ptr<Dependence>> *Dependences =
      new SmallVector<std::unique_ptr<Dependence>, 16>();

  SmallVectorImpl<Instruction *>::iterator MI, ME;
  Instruction *Src = I;
  for (MI = MemInst->begin(), ME = MemInst->end(); MI != ME; ++MI) {
    Instruction *Des = dyn_cast<Instruction>(*MI);

    if (Src == Des) {
      continue;
    }

    if (isa<LoadInst>(Src) && isa<LoadInst>(Des) ||
        isa<StoreInst>(Src) && isa<StoreInst>(Des)) {
      continue;
    }

    bool PossiblyLoopIndependent = true;
    std::unique_ptr<Dependence> D =
        DA->depends(Src, Des, PossiblyLoopIndependent);
    if (D) {
      Dependences->push_back(std::unique_ptr<Dependence>(std::move(D)));
    }
  }

  LCDCache[std::make_pair(L, I)] = Dependences;
  return Dependences;
}

//===----------------------------------------------------------------------===//
// BasicLCDAnalysis implementation
//

char LoopCarriedDependencyAnalysisWrapperPass::ID = 0;

bool LoopCarriedDependencyAnalysisWrapperPass::runOnFunction(Function &F) {
  if (LCDAnalysis) {
    delete LCDAnalysis;
  }

  LCDAnalysis =
      new BasicLCDAnalysis(&getAnalysis<DependenceAnalysis>(),
                           &getAnalysis<LoopInfoWrapperPass>().getLoopInfo());
  LCDAnalysis->setup(F);
  return false;
}

void LoopCarriedDependencyAnalysisWrapperPass::getAnalysisUsage(
    AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<LoopInfoWrapperPass>();
  AU.addRequired<DependenceAnalysis>();
}

void LoopCarriedDependencyAnalysisWrapperPass::
    initializeLoopCarriedDependencyAnalysisWrapperPass() {}

static RegisterPass<LoopCarriedDependencyAnalysisWrapperPass>
    X("lcd-analysis", "Loop-carried dependency analysis", true, true);
