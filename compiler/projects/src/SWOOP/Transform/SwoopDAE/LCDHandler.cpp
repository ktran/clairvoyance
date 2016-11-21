//===--------------- LCDHandler.cpp - LCD Helper fro SWOOP-----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file LCDHandler.cpp
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// This file is a helper class containing functionality to handle LCD.
//
//===----------------------------------------------------------------------===//
#include "LCDHandler.h"

#include <queue>
#include "Util/Analysis/AliasUtils.h"

#undef LCD_BASED_DISAMBIGUATION

#ifdef LCD_BASED_DISAMBIGUATION
static const char *LCD_TAG = "LCD";
static const char *LCD_DISTANCE_TAG = "Distance";
#endif


AliasResult aliasWithStore(AliasAnalysis *AA, LoadInst *LInst, Loop *L) {
  BasicBlock *loadBB = LInst->getParent();
  Value *Pointer = LInst->getPointerOperand();
  AliasResult AliasRes = AliasResult::NoAlias;

  // Keep track of which blocks we already visited
  queue < BasicBlock * > BBQ;
  set < BasicBlock * > BBSet;
  BBQ.push(loadBB);
  bool first = true;

  // While the aliasing is not a MustAlias yet (strongest alias)
  while (!BBQ.empty() && AliasRes != AliasResult::MustAlias) {
    BasicBlock *BB = BBQ.front();
    BBQ.pop();

    // Analyze all store instructions that precede the load
    // in this block
    BasicBlock::reverse_iterator RI(LInst->getIterator());
    for (BasicBlock::reverse_iterator iI = first ? RI : BB->rbegin(),
             iE = BB->rend();
         iI != iE; ++iI) {
      if (StoreInst::classof(&(*iI))) {
        StoreInst *SInst = (StoreInst * ) & (*iI);

        switch (pointerAlias(AA, SInst->getPointerOperand(), Pointer,
                             iI->getModule()->getDataLayout())) {
        case AliasResult::NoAlias:break; // Already default value.
        case AliasResult::MayAlias:
          if (AliasRes == AliasResult::NoAlias) {
            AliasRes = AliasResult::MayAlias;
          }
          break;
        case AliasResult::PartialAlias:
          if (AliasRes == AliasResult::NoAlias ||
              AliasRes == AliasResult::MayAlias) {
            AliasRes = AliasResult::PartialAlias;
          }
          break;
        case AliasResult::MustAlias:AliasRes = AliasResult::MustAlias; // Highest value.
          break;
        }
      }
    }

    // Insert all predecessors of BB
    if (BB != L->getHeader()) {
      for (pred_iterator pI = pred_begin(BB), pE = pred_end(BB); pI != pE;
           ++pI) {
        if (BBSet.insert(*pI).second) {
          BBQ.push(*pI);
        }
      }
    }
    first = false;
  }
  return AliasRes;
}

// Returns true if the combined LCD from toCheck is at least the value of toExpect (or even
// more flexible). I.e. MayAlias + NoAlias are MayAlias in combination, which in turn
// is too unflexible for NoAlias, but would return true for MayAlias and MustAlias.
bool expectAtLeast(AliasAnalysis *AA, LoopInfo *LI, set<Instruction *> &toCheck, LCDResult toExpect) {
  set<Instruction *>::iterator I, IE;
  for (I = toCheck.begin(), IE = toCheck.end(); I != IE; ++I) {
    if (LI->getLoopFor((*I)->getParent())) {
      if (getLCDInfo(AA, LI, *I, 0) > toExpect) {
        return false;
      }
    }
  }
  return true;
}

LCDResult getLCDInfo(AliasAnalysis *AA, LoopInfo *LI, Instruction *I, unsigned int UnrollCount) {
  LCDResult LCDRes = LCDResult::NoLCD;

  // There is no loop around this instruction - so there cannot
  // be an lcd
  if (!LI->getLoopFor(I->getParent())) {
    return LCDResult::NoLCD;
  }

  if (!isa<LoadInst>(I)) {
    return LCDResult::NoLCD;
  }

#ifdef LCD_BASED_DISAMBIGUATION
  // Do not consider the lcd analysis for now - only the alias
  // analysis
  if (InstrhasMetadataKind(I, LCD_TAG)) {
     if (InstrhasMetadataKind(I, LCD_DISTANCE_TAG)) {
       long int distance = stol(getInstructionMD(I, LCD_DISTANCE_TAG));
      if (distance <= UnrollCount) {
         LCDRes = fromString(getInstructionMD(I, LCD_TAG));
      }
     }
    else {
       LCDRes = fromString(getInstructionMD(I, LCD_TAG));
     }
   }

   if (LCDRes == LCDResult::MustLCD) {
    return LCDRes;
   }
#endif

  AliasResult Alias =
      aliasWithStore(AA, (LoadInst *)I, LI->getLoopFor(I->getParent()));
  LCDResult LCDStore;
  switch (Alias) {
  case NoAlias:
    LCDStore = LCDResult::NoLCD;
    break;
  case MayAlias:
    LCDStore = LCDResult::MayLCD;
    break;
  case PartialAlias:
  case MustAlias:
    LCDStore = LCDResult::MustLCD;
    break;
  default:
    // None existing: something went wrong? Be conservative.
    LCDStore = LCDResult::MustLCD;
    break;
  }

  LCDRes = LoopCarriedDependencyAnalysis::combineLCD(LCDRes, LCDStore);
  return LCDRes;
}


LCDResult getLCDUnion(AliasAnalysis *AA, LoopInfo *LI, set<Instruction *> &toCombine) {
  LCDResult Res = LCDResult::NoLCD;
  set<Instruction *>::iterator I, IE;
  for (I = toCombine.begin(), IE = toCombine.end(); I != IE; ++I) {
    if (LI->getLoopFor((*I)->getParent())) {
      Res = LoopCarriedDependencyAnalysis::combineLCD(getLCDInfo(AA, LI, *I, 0), Res);
    }
  }

  return Res;
}
