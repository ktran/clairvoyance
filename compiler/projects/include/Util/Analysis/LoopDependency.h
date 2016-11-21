//===-------- LoopDependency.h - Requirements in Loop Iteration --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file LoopDependency.h
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
//  This file contains utility function to retrieve the requirements
//  of an instruction _within_ a loop iteration. Requirements include:
//  1) data dependencies
//  2) control dependencies (mandatory control dependencies)
//
//===----------------------------------------------------------------------===//

#ifndef UTIL_ANALYSIS_LOOPDEPENDENCY_H
#define UTIL_ANALYSIS_LOOPDEPENDENCY_H

#include <list>
#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/IR/IntrinsicInst.h>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <string>

#include "llvm/Transforms/Utils/Cloning.h"

#include "Util/Annotation/MetadataInfo.h"
#include "Util/Analysis/AliasUtils.h"
#include "Util/Analysis/LoopDependency.h"
#include "Util/DAE/DAEUtils.h"

#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/LoopInfo.h"

using namespace std;
using namespace llvm;

namespace util {

  // Computes the _mandatory_ data dependencies for instruction I n_within_ a loop iteration
  void getDeps(AliasAnalysis *AA, LoopInfo *LI, Instruction *I, set<Instruction *> &DepSet, bool followStores = true);

  // Computes the _mandatory_ control dependencies for instruction I _within_ a loop iteration
  void getControlDeps(AliasAnalysis *AA, LoopInfo *LI, Instruction *I, set<Instruction *> &Deps);

  // Computes the _mandatory_ control and data dependencies for instruction I _within_ a loop iteration
  void getRequirementsInIteration(AliasAnalysis *AA, LoopInfo *LI, Instruction *I, set<Instruction *> &DepSet, bool followStores = true);

  // Adds dependencies of the Instructions in Set to DepSet.
  // Dependencies are considered to be the operators of an Instruction
  // with the exceptions of calls. In case a LoadInst is a dependency
  // the coresponding StoreInst is also considered as a dependency
  // as long it does not operate on visible memory.
  // Retrurns false iff a prohibited instruction are required.
  // The contents of Set and DepSet are only reliable if the result
  // is true.
  bool followDeps(AliasAnalysis *AA, set<Instruction *> &Set, set<Instruction *> &DepSet, bool followStores = true, bool followCalls = true);

  // Convenience call 
  bool followDeps(AliasAnalysis *AA, Instruction *Inst, set<Instruction *> &DepSet);

  // Adds the Instructions in F that terminates a BasicBlock to CfgSet.
  void findTerminators(Function &F, set<Instruction *> &CfgSet);
}

#endif
