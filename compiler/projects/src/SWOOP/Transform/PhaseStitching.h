
//===- Phase Stitching -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
/// \file PhaseStitching.h
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#ifndef SWOOP_TRANSFORM_PHASE_STITCHING_H
#define SWOOP_TRANSFORM_PHASE_STITCHING_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/GenericDomTree.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include <set>

using namespace std;
using namespace llvm;


bool stitchAEDecision(Function &F, Function &Optimized, ValueToValueMapTy &VMapRev, AllocaInst *branch_cond,
		      BasicBlock *DecisionBlock,
		      LoopInfo &LI, DominatorTree &DT,
		      string type, int phaseCount);

bool stitch(Function &F, Function &ToAppend, ValueToValueMapTy &VMap, ValueToValueMapTy &VMapRev, LoopInfo &LI, DominatorTree &DT,
            bool forceIncrement, string type, int phaseCount);
void ensureStrictSSA(Function &F, LoopInfo &LI, DominatorTree &DT,
                     vector<BasicBlock *> &PhaseRoots);

#endif
