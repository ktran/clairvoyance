//===---- OptimisticSwoop.cpp - optimistic hoist & reuse ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file OptimisticSwoop.cpp
///
/// \brief Optimistic version of Swoop. Hoists no- and may-aliases.
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
//  Optimistic version of Swoop. Hoists no- and may-aliases. Which loads
//  are hoisted depends on the flavour:
//  - Aggressive Swoop
//  - Speculative Swoop
//
//===----------------------------------------------------------------------===//

#include "SWOOP/Transform/SwoopDAE/BasicSwoop.h"
#include "../SwoopDAE/LCDHandler.h"
#include "../SwoopDAE/FindInstructions.h"


using namespace util;

namespace swoop {

typedef SmallVector<int, 16> accLCDTy;

struct OptimisticSwoop : public SwoopDAE {
  static char ID;
  OptimisticSwoop(int maxMayLCDInChain = INT_MAX) : SwoopDAE() {
    maxMayLCD = maxMayLCDInChain;
  }

protected:
  void filterLoadsOnLCD(AliasAnalysis *AA,
                          LoopInfo *LI,
                          list<LoadInst *> &Loads,
                          list<LoadInst *> &FilteredLoads,
                          unsigned int UnrollCount) override;
  void divideLoads(list<LoadInst *> &toHoist,
                     list<LoadInst *> &toPref,
                     list<LoadInst *> &toReuse,
                     list<LoadInst *> &toLoad,
                     unsigned int UnrollCount) override;

private:
  std::map<Instruction *, accLCDTy *> AccLCDInfo;

  int maxMayLCD;
  void exploreDepsOnLCD(Instruction *I, accLCDTy &accumulatedLCD, unsigned int UnrollCount);
};


void OptimisticSwoop::divideLoads(list<LoadInst *> &toHoist,
                                  list<LoadInst *> &toPref,
                                  list<LoadInst *> &toReuse,
                                  list<LoadInst *> &toLoad,
                                  unsigned int UnrollCount) {
  int numUnsafePrefetch = 0, numUnsafeLoad = 0;
  set<Instruction *> AllDeps;
  for (auto L : toHoist) {
    getRequirementsInIteration(AA, LI, L, AllDeps);
  }

  for (auto L : toHoist) {
    set<Instruction *> Deps;
    getRequirementsInIteration(AA, LI, L, Deps);

    bool depNoLCD = expectAtLeast(AA, LI, Deps, LCDResult::NoLCD);
    LCDResult SelfLCDRes = getLCDInfo(AA, LI, L, UnrollCount);

    if (SelfLCDRes == LCDResult::NoLCD && depNoLCD) {
      // reuse everything that has no mayLCDs
      // in its requirements
      toReuse.push_back(L);
    } else if (AllDeps.find(L) != AllDeps.end()) {
      // load everything that is required for any
      // other load to hoist
      toLoad.push_back(L);
    } else {
      // otherwise prefetch: some mayLcd in its
      // requirements
      toPref.push_back(L);
    }
  }
}

void OptimisticSwoop::filterLoadsOnLCD(AliasAnalysis *AA,
                                       LoopInfo *LI,
                                       list<LoadInst *> &Loads,
                                       list<LoadInst *> &FilteredLoads,
                                       unsigned int UnrollCount) {
  list<LoadInst *>::iterator L = Loads.begin();
  while (L != Loads.end()) {

    set<Instruction *> Deps;
    getRequirementsInIteration(AA, LI, *L, Deps);
    LCDResult selfLCD = getLCDInfo(AA, LI, *L, UnrollCount);
    bool depsNoLCD = expectAtLeast(AA, LI, Deps, LCDResult::NoLCD);

    if (selfLCD == LCDResult::MustLCD) {
      ++L;
      continue;
    }

    // If dependencies are no LCDs, and this load is a
    // may or no lcd, it can be reused/prefetched
    if (depsNoLCD && selfLCD < LCDResult::MustLCD) {
      FilteredLoads.push_back(*L);
      ++L;
      continue;
    }

    bool depsMayOrNoLCD = expectAtLeast(AA, LI, Deps, LCDResult::MayLCD);
    if (!depsMayOrNoLCD) {
      // dependencies contain must lcd, don't include load
      ++L;
      continue;
    }

    // Optimistic hoisting.
    bool operandsMeetReq = true;
    for (User::value_op_iterator I = (*L)->value_op_begin(),
                                 IE = (*L)->value_op_end();
         I != IE; ++I) {
      if (!Instruction::classof(*I)) {
        continue;
      }

      Instruction *Inst = (Instruction *)(*I);
      LCDResult LCDRes = getLCDInfo(AA, LI, Inst, UnrollCount);

      accLCDTy accLCDInfo;
      exploreDepsOnLCD(Inst, accLCDInfo, UnrollCount);

      // Compute number of may lcds on the chain
      int maxMayLCDInChain =
          *std::max_element(accLCDInfo.begin(), accLCDInfo.end());
      if (LCDRes != LCDResult::NoLCD) {
        ++maxMayLCDInChain;
      }

      // Check if chain meets requirements of max maylcds
      if (maxMayLCDInChain > maxMayLCD) {
        operandsMeetReq = false;
        break;
      }
    }

    if (operandsMeetReq) {
      FilteredLoads.push_back(*L);
    }

    ++L;
  }
}

void OptimisticSwoop::exploreDepsOnLCD(Instruction *I, accLCDTy &accumulatedLCD, unsigned int UnrollCount) {
  LCDResult LCDRes = getLCDInfo(AA, LI, I, UnrollCount);
  assert(LCDRes != LCDResult::MustLCD &&
         "No Must lcd is allowed to pass this!");

  int addLCD = 0;
  if (LCDRes == LCDResult::MayLCD) {
    addLCD = 1;
  }

  int index = 0;
  for (User::value_op_iterator U = I->value_op_begin(), UE = I->value_op_end();
       U != UE; ++U, ++index) {
    if (isa<Instruction>(*U) && !isa<PHINode>(*U)) {
      Instruction *Inst = (Instruction *)*U;
      accLCDTy operandAccInfo;
      if (AccLCDInfo.find(Inst) != AccLCDInfo.end()) {
        operandAccInfo = *(AccLCDInfo[Inst]);
      } else {
        exploreDepsOnLCD(Inst, operandAccInfo, UnrollCount);
        AccLCDInfo[Inst] =
            new accLCDTy{operandAccInfo.begin(), operandAccInfo.end()};
      }

      accumulatedLCD.push_back(
          *std::max_element(operandAccInfo.begin(), operandAccInfo.end()) +
          addLCD);
    }
  }

  if (accumulatedLCD.size() == 0) {
    accumulatedLCD.push_back(addLCD);
  }
}

char OptimisticSwoop::ID = 0;
static RegisterPass<OptimisticSwoop>
    C("optimistic-swoop", "Optimistically hoisting loads into access phase.",
      false, false);

//===----------------------------------------------------------------------===//
// AggressiveSwoop implementation: hoist all no & may LCDs, and only reuse the
// safe ones.

namespace {
struct AggressiveSwoop : public OptimisticSwoop {
  static char ID;
  AggressiveSwoop() : OptimisticSwoop(INT_MAX) {}
};
}

char AggressiveSwoop::ID = 0;
static RegisterPass<AggressiveSwoop> D("aggressive-swoop",
                                       "Hoisting all loads.", false, false);

//===----------------------------------------------------------------------===//
// SpeculativeSwoop implementation: hoist all no & may LCDs, and reuse all.

namespace {
struct SpeculativeSwoop : public OptimisticSwoop {
  static char ID;
  SpeculativeSwoop() : OptimisticSwoop(INT_MAX) {}

protected:
  void divideLoads(list<LoadInst *> &toHoist,
                     list<LoadInst *> &toPref,
                     list<LoadInst *> &toReuse,
                     list<LoadInst *> &toLoad,
                     unsigned int UnrollCount) override;
  LCDResult acceptedForReuse() override;
};
}

void SpeculativeSwoop::divideLoads(list<LoadInst *> &toHoist,
                                   list<LoadInst *> &toPref,
                                   list<LoadInst *> &toReuse,
                                   list<LoadInst *> &toLoad,
                                   unsigned int UnrollCount) {
  toReuse.insert(toReuse.end(), toHoist.begin(), toHoist.end());
}

  LCDResult SpeculativeSwoop::acceptedForReuse() {
    return LCDResult::MayLCD;
  }

char SpeculativeSwoop::ID = 0;
static RegisterPass<SpeculativeSwoop>
    E("speculative-swoop", "Hoisting and reusing all loads.", false, false);

//===----------------------------------------------------------------------===//
// SmartDAE implementation: hoist all no & may LCDs, prefetch all, and reuse none.

namespace {
struct SmartDAE : public OptimisticSwoop {
  static char ID;
  SmartDAE() : OptimisticSwoop(INT_MAX) {}

protected:
  void divideLoads(list<LoadInst *> &toHoist,
                     list<LoadInst *> &toPref,
                     list<LoadInst *> &toReuse,
                     list<LoadInst *> &toLoad,
                     unsigned int UnrollCount) override;
  void selectInstructionsToReuseInExecute(Function *F, set<Instruction*> *toKeep, set<Instruction*> *toUpdateForNextPhases,
                                          LCDResult MinLCDRequirement, bool ReuseAll, bool ReuseBranchCondition) override ;

};
}

void SmartDAE::divideLoads(list<LoadInst *> &toHoist,
                           list<LoadInst *> &toPref,
                           list<LoadInst *> &toReuse,
                           list<LoadInst *> &toLoad,
                           unsigned int UnrollCount) {
  toPref.insert(toPref.end(), toHoist.begin(), toHoist.end());
}

void SmartDAE::selectInstructionsToReuseInExecute(Function *F, set<Instruction*> *toKeep, set<Instruction*> *toUpdateForNextPhases,
                                        LCDResult MinLCDRequirement, bool ReuseAll, bool ReuseBranchCondition) {
  BasicBlock &EntryBlock = F->getEntryBlock();
  for (BasicBlock::iterator I = EntryBlock.begin(), IE = EntryBlock.end(); I != IE; ++I) {
    if (toKeep->insert(&*I).second) {
      toUpdateForNextPhases->insert(&*I);
    }
  }
}

char SmartDAE::ID = 0;
static RegisterPass<SmartDAE>
    F("smartdae", "Hoisting and prefetching all may & no aliases. Reuse none.", false, false);
}

