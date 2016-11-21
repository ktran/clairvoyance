//===- LoopUtils.cpp - Utility for LoopChunking----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file LoopUtils.cpp
///
/// \brief Utility for LoopChunking
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//===----------------------------------------------------------------------===//
#include "DAE/Utils/SkelUtils/headers.h"
#include "HandleVirtualIterators.cpp"
#include "Util/Annotation/MetadataInfo.h"

using namespace llvm;
using namespace std;
using namespace util;

#ifndef LoopUtils_
#define LoopUtils_

BasicBlock *insertArtificialLoopLatch(Loop *L, LoopInfo *LI);
bool isGuaranteedToExecute(Instruction *Inst, Loop *CurLoop, DominatorTree *DT);
void updatePHInodes(BasicBlock *b, BasicBlock *oldP, BasicBlock *newP);
void replaceBrupdatePhi(BasicBlock *&BB, BasicBlock *&o, BasicBlock *&n);

/* to treat while and for loops unitary, create an artificial loop latch IF
 * NECESSARY*/
BasicBlock *insertArtificialLoopLatch(Loop *L, LoopInfo *LI) {
  BasicBlock *oldB = L->getLoopLatch();
  if (oldB)
    return oldB;

  // avoid duplicates: do not insert an artificial latch if there is already one
  size_t found;
  std::string text = "_latch";
  if (oldB != 0) {
    found = oldB->getName().find(text);
    if (found != std::string::npos)
      return oldB;
  }

  BasicBlock *latchBB =
      BasicBlock::Create(L->getHeader()->getContext(),
                         Twine(L->getHeader()->getName().str() + "_latch"),
                         L->getHeader()->getParent(), L->getHeader());
  std::vector<BasicBlock *> oldLat;

  // if there is more than one BB latches, getLoopLatch() returns O
  if (oldB == 0) {

    BranchInst::Create(L->getHeader(), latchBB);
    std::vector<BasicBlock *> v = L->getBlocks();
    BasicBlock *header = L->getHeader();

    pred_iterator PI = pred_begin(L->getHeader()), E = pred_end(L->getHeader());
    while (PI != E) {
      if (belongs(v, *PI)) {
        BasicBlock *bpi = dyn_cast<BasicBlock>(*PI);
        replaceBrupdatePhi(bpi, header, latchBB);
        oldLat.push_back(*PI);
        updatePHInodes(L->getHeader(), *PI, latchBB);
        PI = pred_begin(L->getHeader());
      } else
        ++PI;
    }
  }
  // update loop info

  L->addBasicBlockToLoop(latchBB, *LI);

  return latchBB;
}

/* update the PHI nodes whenever a terminating instruction is changed*/
void updatePHInodes(BasicBlock *b, BasicBlock *oldP, BasicBlock *newP) {
  BasicBlock::iterator i = b->begin();

  while (isa<PHINode>(i)) {
    i->replaceUsesOfWith(oldP, newP);
    i++;
  }
}

/*
             BB
             |
             |
           x---
           |   |
          o     n
 */
void replaceBrupdatePhi(BasicBlock *&BB, BasicBlock *&o, BasicBlock *&n) {
  // o->removePredecessor(BB, false);
  BB->getTerminator()->replaceUsesOfWith(o, n);
  // updateDT(BB, n);
  BasicBlock::iterator i = n->begin();

  while (isa<PHINode>(i)) {
    errs() << "TODO update PHI !! \n\n\n";
    // how to add a new pred, if there is no entry for it in the PHI node
    i++;
  }
}

bool isGuaranteedToExecute(Instruction *Inst, Loop *CurLoop,
                           DominatorTree *DT) {
  if (!CurLoop)
    return true;

  // Otherwise we have to check to make sure that the instruction dominates all
  // of the exit blocks.  If it doesn't, then there is a path out of the loop
  // which does not execute this instruction, so we can't hoist it.

  // If the instruction is in the header block for the loop (which is very
  // common), it is always guaranteed to dominate the exit blocks.  Since this
  // is a common case, and can save some work, check it now.
  if (Inst->getParent() == CurLoop->getHeader())
    return true;

  // Get the exit blocks for the current loop.
  SmallVector<BasicBlock *, 8> ExitBlocks;
  CurLoop->getExitBlocks(ExitBlocks);

  // Verify that the block dominates each of the exit blocks of the loop.
  for (unsigned i = 0, e = ExitBlocks.size(); i != e; ++i)
    if (!DT->dominates(Inst->getParent(), ExitBlocks[i]))
      return false;

  // As a degenerate case, if the loop is statically infinite then we haven't
  // proven anything since there are no exit blocks.
  if (ExitBlocks.empty())
    return false;

  return true;
}

#endif
