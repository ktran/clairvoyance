//===------ AliasUtils.h - Utils for alias analysis ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file AliasUtils.hy
///
/// \brief Utilities for alias analysis
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
//
//===----------------------------------------------------------------------===//

#ifndef UTIL_ANALYSIS_ALIASUTILS_H
#define UTIL_ANALYSIS_ALIASUTILS_H

#include <list>

#include "Util/Annotation/MetadataInfo.h"

#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/IR/IntrinsicInst.h>

using namespace std;
using namespace llvm;

namespace util {
  // Checks if two pointers alias
  AliasResult pointerAlias(AliasAnalysis *AA, Value *P1, Value *P2, const DataLayout &DL);

  // Returns the closest alias between store and any of the LoadInsts
  // in toPref.
  AliasResult crossCheck(AliasAnalysis *AA, StoreInst *store, list<LoadInst *> &toPref);

  // Anotates stores in fun with the closest alias type to
  // any of the loads in toPref. (To be clear alias analysis are
  // performed between the address of each store and the address
  // of each load.) Results are annotated as metadata.
  void anotateStores(AliasAnalysis *AA, Function &fun, list<LoadInst *> &toPref);
}

#endif
