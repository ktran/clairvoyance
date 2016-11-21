//===- LoopCarriedDependencyAnalysis.h - LCD Analysis Interface -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file LoopCarriedDependencyAnalysis.h
///
/// \brief LCD Analysis Interface
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// This file defines the generic LoopCarriedDependencyAnalysis interface, which
// is used as the common interface used by all clients of loop carried
// dependency
// information, and implemented by all loop carried dependency analysis
//  implementations.
//
//===----------------------------------------------------------------------===//

#ifndef UTIL_ANALYSIS_LCDANALYSIS_H
#define UTIL_ANALYSIS_LCDANALYSIS_H

#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/PassRegistry.h"
#include <algorithm>

using namespace llvm;

namespace util {

enum LCDResult {
  NoLCD = 0,

  MayLCD,

  MustLCD,

  END

};

static const int LCDResultCount = LCDResult::END - LCDResult::NoLCD;
static const std::string LCDStrings[] = {"NoLCD", "MayLCD", "MustLCD"};
static const std::string getStringRep(int enumVal) { return LCDStrings[enumVal]; }

static LCDResult fromString(std::string representation) {
  for (int i = 0; i < LCDResultCount; ++i) {
    if (LCDStrings[i].compare(representation) == 0) {
      return static_cast<LCDResult>(i);
    }
  }

  return LCDResult::END;
}

class LoopCarriedDependencyAnalysis {

public:
  bool isNoLCD(Instruction *I, const Loop *L) {
    return checkLCD(I, L) == NoLCD;
  }

  bool isMustLCD(Instruction *I, const Loop *L) {
    return checkLCD(I, L) == MustLCD;
  }

  bool isMayLCD(Instruction *I, const Loop *L) {
    return checkLCD(I, L) == MayLCD;
  }

  static LCDResult combineLCD(LCDResult A, LCDResult B) {
      return static_cast<LCDResult>(std::max(A, B));
  }
  
  virtual const LCDResult checkLCD(Instruction *I, const Loop *L) = 0;
  virtual bool getLCDDistance(Instruction *I, const Loop *L,
                              long int &Distance) = 0;
  virtual void setup(Function &F) = 0;

protected:
  bool collectMemInst(const Loop &L, SmallVectorImpl<Instruction *> &MemInst) {
      for (Loop::block_iterator BB = L.block_begin(), BE = L.block_end(); BB != BE;
       ++BB) {
    for (BasicBlock::iterator I = (*BB)->begin(), E = (*BB)->end(); I != E;
         ++I) {
      Instruction *Ins = dyn_cast<Instruction>(I);
      if (!Ins) {
        continue;
      }

      if (isa<LoadInst>(I) || isa<StoreInst>(I)) {
        MemInst.push_back(&*I);
      }
    }
  }

  return true;
  }
};

class LoopCarriedDependencyAnalysisWrapperPass : public FunctionPass {
private:
  LoopCarriedDependencyAnalysis *LCDAnalysis;

public:
  static char ID;

  LoopCarriedDependencyAnalysisWrapperPass()
      : FunctionPass(ID), LCDAnalysis(NULL) {
    initializeLoopCarriedDependencyAnalysisWrapperPass();
  }

  ~LoopCarriedDependencyAnalysisWrapperPass() {
    if (LCDAnalysis) {
      delete LCDAnalysis;
    }
  }

  LoopCarriedDependencyAnalysis &getLCDAnalysis() { return *LCDAnalysis; };
  const LoopCarriedDependencyAnalysis &getLCDAnalysis() const {
    return *LCDAnalysis;
  };

  bool runOnFunction(Function &F) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;

private:
  virtual void initializeLoopCarriedDependencyAnalysisWrapperPass();
};

} // End namespace

#endif
