//===- LoopExtract.cpp - Extracts DAE-targeted loop----
//--------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file LoopExtract.cpp
///
/// \brief Extracts DAE-targeted loop
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//===----------------------------------------------------------------------===//
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/CodeExtractor.h"

#include <fstream>

#define F_KERNEL_SUBSTR "__kernel__"
#define PROLOGUE_SUBSTR "prol"

using namespace llvm;

#include "../SkelUtils/Utils.cpp"

static cl::opt<std::string> BenchName("bench-name",
                                      cl::desc("The benchmark name"),
                                      cl::value_desc("name"));

static cl::opt<bool> IsDae("is-dae",
                           cl::desc("Use depth-based DAE loop detection"));

namespace {
struct LoopExtract : public LoopPass {
  static char ID; // Pass identification, replacement for typeid

  LoopExtract() : LoopPass(ID) {}
  unsigned NumLoops;

  virtual bool runOnLoop(Loop *L, LPPassManager &LPM);

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequiredID(BreakCriticalEdgesID);
    AU.addRequiredID(LoopSimplifyID);
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
  }

  void PrintModule(Function *F);
  void PrintFunction(Function *F, std::string suf);
  BasicBlock *getCaller(Function *F);
  bool toBeExtracted(Loop *L);
};
}

char LoopExtract::ID = 0;
static RegisterPass<LoopExtract>
    X("second-loop-extract", "Extract second level loops into new functions",
      true, true);

bool LoopExtract::runOnLoop(Loop *L, LPPassManager &LPM) {
  // if already extracted
  Function *F = L->getHeader()->getParent();
  if (!toBeDAE(F)) {
    return false;
  }

  // find the chunked loops
  if (IsDae) {
    // parent loop has to be marked
    bool isMarked = L->getParentLoop() &&
      L->getParentLoop()->getHeader()->getName().str().find(F_KERNEL_SUBSTR) != string::npos;
    if (!isMarked) {
      return false;
    }
  } else {
    if (!toBeExtracted(L)) {
      return false;
    }
  }

  // If LoopSimplify form is not available, stay out of trouble.
  if (!L->isLoopSimplifyForm()) {
    return false;
  }

  DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  bool Changed = false;

  // If there is more than one top-level loop in this function, extract all of
  // the loops. Otherwise there is exactly one top-level loop; in this case if
  // this function is more than a minimal wrapper around the loop, extract
  // the loop.
  bool ShouldExtractLoop = false;

  // Extract the loop if the entry block doesn't branch to the loop header.
  TerminatorInst *EntryTI =
      L->getHeader()->getParent()->getEntryBlock().getTerminator();
  if (!isa<BranchInst>(EntryTI) ||
      !cast<BranchInst>(EntryTI)->isUnconditional() ||
      EntryTI->getSuccessor(0) != L->getHeader()) {
    ShouldExtractLoop = true;
  } else {
    // Check to see if any exits from the loop are more than just return
    // blocks.
    SmallVector<BasicBlock *, 8> ExitBlocks;
    L->getExitBlocks(ExitBlocks);
    for (unsigned i = 0, e = ExitBlocks.size(); i != e; ++i)
      if (!isa<ReturnInst>(ExitBlocks[i]->getTerminator())) {
        ShouldExtractLoop = true;
        break;
      }
  }

  if (!ShouldExtractLoop) {
    // Loop is already a function, it is actually not necessary to extract the
    // loop.
    ShouldExtractLoop = true;
  }

  if (ShouldExtractLoop) {
    // We must omit landing pads. Landing pads must accompany the invoke
    // instruction. But this would result in a loop in the extracted
    // function. An infinite cycle occurs when it tries to extract that loop as
    // well.
    SmallVector<BasicBlock *, 8> ExitBlocks;
    L->getExitBlocks(ExitBlocks);
    for (unsigned i = 0, e = ExitBlocks.size(); i != e; ++i)
      if (ExitBlocks[i]->isLandingPad()) {
        ShouldExtractLoop = false;
        break;
      }
  }

  if (ShouldExtractLoop) {

    CodeExtractor Extractor(DT, *L);
    Function *nF = Extractor.extractCodeRegion();
    if (nF != 0) {
      BasicBlock *codeRepl = getCaller(nF);
      nF->addFnAttr(Attribute::AlwaysInline);

      Changed = true;

      LoopInfo *LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
      if (L->getParentLoop()) {
        L->getParentLoop()->addBasicBlockToLoop(codeRepl, *LI);
      }

      // After extraction, the loop is replaced by a function call, so
      // we shouldn't try to run any more loop passes on it.
      LI->markAsRemoved(L);
    }
  }

  return Changed;
}

bool LoopExtract::toBeExtracted(Loop *L) {
  bool isMarked =
      L->getHeader()->getName().str().find(F_KERNEL_SUBSTR) != string::npos;
  bool isOriginalLoop =
      L->getHeader()->getName().str().find(PROLOGUE_SUBSTR) == string::npos;
  bool functionNotYetExtracted =
      L->getHeader()->getParent()->getName().str().find(F_KERNEL_SUBSTR) ==
      string::npos;

  if (!isMarked || !isOriginalLoop || !functionNotYetExtracted) {
    return false;
  }
  return true;
}

BasicBlock *LoopExtract::getCaller(Function *F) {
  for (Value::user_iterator I = F->user_begin(), E = F->user_end(); I != E;
       ++I) {
    if (isa<CallInst>(*I) || isa<InvokeInst>(*I)) {
      Instruction *User = dyn_cast<Instruction>(*I);
      return User->getParent();
    }
  }
  return 0;
}
