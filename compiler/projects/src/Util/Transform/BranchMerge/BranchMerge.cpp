//===----BranchMerge.cpp - Early evaluation of branches and later merge----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file BranchMerge.cpp
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// This file provides functionality to evaluate branches at an early stage,
// and to decide whether to run the optimized (merged branches) or unoptimized
// (original) version.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"

#include "llvm/Transforms/Utils/Cloning.h"


#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "Util/Transform/BranchMerge/BranchMerge.h"

#define F_KERNEL_SUBSTR "__kernel__"
#define EXECUTE_SUFFIX "_execute"
#define CLONE_SUFFIX "_clone"

BasicBlock *deepCopyBB(BasicBlock *src) {
  BasicBlock *dst = BasicBlock::Create(src->getContext());
  for (BasicBlock::iterator iI = src->begin(), iE = src->end(); iI != iE; ++iI) {
    Instruction *c = iI->clone();
    dst->getInstList().push_back(c);
  }
  return dst;
}

void replaceBranch(BasicBlock *block, double THRESHOLD) {
  TerminatorInst *TInst = block->getTerminator();
  IRBuilder<> Builder(TInst);
  BranchInst *uncondBI;
  BasicBlock *dst, *comp;
  if (BranchInst *BI = dyn_cast<BranchInst>(TInst)) {
    std::pair<bool, int> res = isReducableBranch(BI, THRESHOLD);
    if (res.first) {
      if (res.second == 0) {
        dst = BI->getSuccessor(0);
        comp = BI->getSuccessor(1);
        uncondBI = BranchInst::Create(dst);
        AttachMetadata(uncondBI, "BranchProb0", "1");
        AttachMetadata(uncondBI, "BranchProb1", "0");
        comp->removePredecessor(block);
        ReplaceInstWithInst(TInst, uncondBI);
      } else if (res.second == 1) {
        dst = BI->getSuccessor(1);
        comp = BI->getSuccessor(0);
        uncondBI = BranchInst::Create(dst);
        AttachMetadata(uncondBI, "BranchProb0", "0");
        AttachMetadata(uncondBI, "BranchProb1", "1");
        comp->removePredecessor(block);
        ReplaceInstWithInst(TInst, uncondBI);
      }
    }
  }
}

bool hasEnding (std::string fullString, std::string ending) {
  if (fullString.length() >= ending.length()) {
    return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
  } else {
    return false;
  }
}

bool minimizeFunctionFromBranchPred(LoopInfo *LI, Function *cF, double threshold) {
  errs() << "Optimizing function: " << cF->getName().str() << "\n";

  std::vector<Loop *> Loops(LI->begin(), LI->end());
  assert("Only expecting one loop." && Loops.size() == 1);
  Loop *L = *(Loops.begin());
  BasicBlock *Latch = L->getLoopLatch();
  BasicBlock *CondBranchBB = (Latch->getUniquePredecessor()) ? Latch->getUniquePredecessor() : Latch;

  for (Function::iterator block = cF->begin(), blockEnd = cF->end(); block != blockEnd; ++block) {
    BasicBlock *BB = &*block;
    if (CondBranchBB == BB) {
      continue;
    }

    for (BasicBlock::iterator I = BB->begin(), IE = BB->end(); I != IE; ++I) {
      if (InstrhasMetadataKind(&*I, "SwoopType") && "DecisionBlock" == getInstructionMD(&*I, "SwoopType")) {
        replaceBranch(BB, threshold);
        break;
      }
    }
  }
  return true;
}

bool isFKernel(Function &F) {
  return
      F.getName().str().find(F_KERNEL_SUBSTR) != std::string::npos;
}
