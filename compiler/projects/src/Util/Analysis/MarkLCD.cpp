//===--------------- MarkLCD.cpp - Marking loop carried dependencies-------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file MarkLCD.cpp
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// This file annotates memory operations in respect of loop carried
// dependencies:
// NoLCD, MayLCD, MustLCD.
//
//
//===----------------------------------------------------------------------===//

#include "Util/Analysis/LoopCarriedDependencyAnalysis.h"
#include "Util/Annotation/MetadataInfo.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"

using namespace llvm;
using namespace std;
using namespace util;

#define F_KERNEL_SUBSTR "__kernel__"

namespace {
struct MarkLCD : public FunctionPass {
  static char ID;
  MarkLCD() : FunctionPass(ID) {}

public:
  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<LoopCarriedDependencyAnalysisWrapperPass>();
  }

  bool runOnFunction(Function &F);

private:
  LoopInfo *LI;
  LoopCarriedDependencyAnalysis *LCDAnalysis;
};
}

bool MarkLCD::runOnFunction(Function &F) {
  LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  LCDAnalysis =
      &getAnalysis<LoopCarriedDependencyAnalysisWrapperPass>().getLCDAnalysis();
  bool changed = false;

  std::queue<Loop *> loops;
  for (LoopInfo::iterator L = LI->begin(), LE = LI->end(); L != LE; ++L) {
    loops.push((*L));
  }

  while (!loops.empty()) {
    Loop *L = loops.front();
    loops.pop();

    for (auto SL : L->getSubLoops()) {
      loops.push(SL);
    }

    if (L->getHeader()->getName().find(F_KERNEL_SUBSTR) == string::npos) {
      continue;
    }

    changed = true;

    for (Loop::block_iterator B = L->block_begin(), BE = L->block_end();
         B != BE; ++B) {
      for (BasicBlock::iterator I = (*B)->begin(), IE = (*B)->end(); I != IE;
           ++I) {
        LCDResult LCDRes = LCDAnalysis->checkLCD(&(*I), L);
        AttachMetadata(&(*I), "LCD", getStringRep(LCDRes));
        long int Distance;
        bool distanceCorrect = LCDAnalysis->getLCDDistance(&(*I), L, Distance);
        if (distanceCorrect) {
          AttachMetadata(&(*I), "Distance", to_string(Distance));
        }
      }
    }
  }
  return changed;
}

char MarkLCD::ID = 0;
static RegisterPass<MarkLCD> A("mark-lcd",
                               "Marking memory operations with LCD information",
                               false, false);
