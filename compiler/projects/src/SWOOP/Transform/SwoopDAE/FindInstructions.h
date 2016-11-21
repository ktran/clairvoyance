//===----------------------- FindInstructions.h --------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file FindInstructions.h
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// This file is a helper class containing functionality to find instructions
// to hoist.
//
//===----------------------------------------------------------------------===////
#ifndef PROJECT_FINDINSTRUCTIONS_H
#define PROJECT_FINDINSTRUCTIONS_H

#include <list>
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include <llvm/Analysis/BasicAliasAnalysis.h>
#include "SWOOP/Transform/SwoopDAE/BasicSwoop.h"

using namespace llvm;
using namespace std;

void findAccessInsts(AliasAnalysis *AA, LoopInfo *LI, Function &fun, list<LoadInst *> &toHoist, bool HoistDelinquent,
                     unsigned int IndirThresh);
void findRelevantLoads(Function &F, list<LoadInst *> &LoadList, bool HoistDelinquent);

#endif //PROJECT_FINDINSTRUCTIONS_H
