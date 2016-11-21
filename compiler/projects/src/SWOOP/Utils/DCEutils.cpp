//===- DCEutils.cpp - Kill unsafe stores with backups --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file DCEutils.cpp
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// Description of pass ...
//
//===----------------------------------------------------------------------===//
#include "llvm/Transforms/Utils/Local.h"

bool SimplifyCFGperFunction(Function *F, TargetTransformInfo &TTI,
                            unsigned bonusInstThreshold) {

  bool modif = false;
  Function::iterator bbI = F->begin(), bbE = F->end();
  while (bbI != bbE) {
    // Function::iterator helper(bbI);
    modif = SimplifyCFG(&*bbI, TTI, bonusInstThreshold);
    if (modif)
      bbI = F->begin(); // helper;
    else
      bbI++;
  }
}

bool SimplifyCFGExclude(Function *F, TargetTransformInfo &TTI,
                        unsigned bonusInstThreshold,
                        vector<BasicBlock *> excludeList) {

  bool modif = false;
  Function::iterator bbI = F->begin(), bbE = F->end();
  while (bbI != bbE) {
    if (std::find(excludeList.begin(), excludeList.end(), &*bbI) ==
        excludeList.end()) {
      modif = SimplifyCFG(&*bbI, TTI, bonusInstThreshold);
    } else {
      modif = false;
    }
    if (modif)
      bbI = F->begin(); // helper;
    else
      bbI++;
  }
}

void simplifyCFG(Function *F, TargetTransformInfo &TTI) {
  // simplify the CFG of A to remove dead code
  vector<BasicBlock *> excludeInCfg;
  excludeInCfg.push_back(&(F->getEntryBlock()));
  excludeInCfg.push_back(F->getEntryBlock().getTerminator()->getSuccessor(0));

  SimplifyCFGExclude(F, TTI, 0, excludeInCfg);
}

