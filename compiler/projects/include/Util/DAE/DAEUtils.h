//===-------- DAEUtils.h - Utils for creating DAE-like loops -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file DAEUtils.h
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
//===----------------------------------------------------------------------===//

#ifndef UTIL_DAE_DAEUTILS_H
#define UTIL_DAE_DAEUTILS_H

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

#include "llvm/IR/IRBuilder.h"
#include "llvm/Analysis/CFG.h"

using namespace std;

#define F_KERNEL_SUBSTR "__kernel__"
#define CLONE_SUFFIX "_clone"

namespace util {
  enum PrefInsertResult { Inserted, BadDeps, IndirLimit, Redundant };
  
  // Remove all instructions from F that are in KeepSet
  void removeUnlisted(Function &F, set<Instruction *> &KeepSet);

  // Clones Function F to its parent Module. A pointer to the
  // clone is returned.
  Function* cloneFunction(Function *F);

  // Same as cloneFunction(F), but initialize the value mapping of the clone
  // in VMap
  Function* cloneFunction(Function *F, ValueToValueMapTy &VMap);

  // Replaces arguments of E by A's arguments
  void replaceArgs(Function *E, Function *A);

  // Inserts a prefetch for LInst as early as possible
  // (i.e. as soon as the adress has been computed).
  // The prefetch and all its dependencies will also
  // be inserted in toKeep.
  // Returns the result of the insertion.
  PrefInsertResult
    insertPrefetch(AliasAnalysis *AA, LoadInst *LInst, set<Instruction *> &toKeep,
                 map<LoadInst *, pair<CastInst *, CallInst *>> &prefs,
		 unsigned Threshold);

  // Adds pointer to all LoadInsts in F to LoadList.
  void findLoads(Function &F, list<LoadInst *> &LoadList);

  // Adds LoadInsts in LoadList to VisList if they
  // operate on visible data.
  void findVisibleLoads(list<LoadInst *> &LoadList, list<LoadInst *> &VisList);

  // Returns true
  bool isUnderThreshold(set<Instruction *> Deps, unsigned Threshold);

  // Returns true if value is a function argument
  Value *isFunArgument(Function *E, Function *A, Value *arg);

  // Returns true if Pointer is a global pointer
  bool isNonLocalPointer(Value *Pointer);

  // Returns true iff Pointer does have a local destination.
  bool isLocalPointer(Value *Pointer);
}

#endif
