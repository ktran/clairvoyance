//===--------------- LCDHandler.h - LCD Helper fro SWOOP-----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file LCDHandler.h
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// This file is a helper class containing functionality to handle LCD.
//
//===----------------------------------------------------------------------===//
#ifndef PROJECT_LCDHANDLER_H
#define PROJECT_LCDHANDLER_H

#include <set>
#include "llvm/Analysis/LoopInfo.h"

#include "Util/Analysis/LoopCarriedDependencyAnalysis.h"


using namespace llvm;
using namespace std;
using namespace util;

bool expectAtLeast(AliasAnalysis *AA, LoopInfo *LI, set<Instruction *> &toCheck, LCDResult toExpect);
LCDResult getLCDInfo(AliasAnalysis *AA, LoopInfo *LI, Instruction *I, unsigned int UnrollCount);
LCDResult getLCDUnion(AliasAnalysis *AA, LoopInfo *LI, set<Instruction *> &toCombine);


#endif //PROJECT_LCDHANDLER_H
