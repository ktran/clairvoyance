//===- MarkLoopsToSwoopify.cpp - Kill unsafe stores with backups
//--------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file MarkLoopsToSwoopify.cpp
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// Description of pass ...
//
//===----------------------------------------------------------------------===//
#include "llvm/Analysis/LoopPass.h"

#include "../../../DAE/Utils/SkelUtils/Utils.cpp"

#define KERNEL_MARKING "__kernel__"

using namespace llvm;

static cl::opt<std::string> BenchName("bench-name",
                                      cl::desc("The benchmark name"),
                                      cl::value_desc("name"));

static cl::opt<bool> RequireDelinquent(
    "require-delinquent",
    cl::desc("Loop has to contain delinquent loads to be marked"),
    cl::init(true));

namespace {
struct MarkLoopsToSwoopify : public FunctionPass {
public:
  static char ID;
  MarkLoopsToSwoopify() : FunctionPass(ID) {}

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<DominatorTreeWrapperPass>();
  }

  bool runOnFunction(Function &F);

private:
  unsigned loopCounter = 0;
  bool markLoops(std::vector<Loop *> Loops, DominatorTree &DT);
};
}

bool MarkLoopsToSwoopify::runOnFunction(Function &F) {
  if (!toBeDAE(&F)) {
    return false;
  }

  DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  std::vector<Loop *> Loops(LI.begin(), LI.end());

  return markLoops(Loops, DT);
}

bool MarkLoopsToSwoopify::markLoops(std::vector<Loop *> Loops,
                                    DominatorTree &DT) {
  bool markedLoop = false;

  for (auto I = Loops.begin(), IE = Loops.end(); I != IE; ++I) {
    Loop *L = *I;
    if (loopToBeDAE(L, BenchName, RequireDelinquent)) {
      BasicBlock *H = L->getHeader();
      H->setName(Twine(KERNEL_MARKING + H->getParent()->getName().str() +
                       std::to_string(loopCounter)));
      loopCounter++;
      markedLoop = true;
    }

    std::vector<Loop *> subLoops = L->getSubLoops();
    bool subLoopsMarked = markLoops(subLoops, DT);
    markedLoop = markedLoop || subLoopsMarked;
  }

  return markedLoop;
}

char MarkLoopsToSwoopify::ID = 1;
static RegisterPass<MarkLoopsToSwoopify>
    X("mark-loops", "Mark loops to swoopify pass", true, false);
