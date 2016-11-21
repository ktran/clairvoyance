//===------ AliasUtils.cpp - Utils for alias analysis ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file AliasUtils.cpp
///
/// \brief Utilities for alias analysis
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
//
//===----------------------------------------------------------------------===//
#include "Util/Analysis/AliasUtils.h"

namespace util {
  static void findStores(Function &F, list<StoreInst *> &StoreList) {
    for (inst_iterator iI = inst_begin(F), iE = inst_end(F); iI != iE; ++iI) {
      if (StoreInst::classof(&(*iI))) {
        StoreList.push_back((StoreInst *)&(*iI));
      }
    }
  }
  
  AliasResult pointerAlias(AliasAnalysis *AA, Value *P1, Value *P2, const DataLayout &DL) {
    uint64_t P1Size = MemoryLocation::UnknownSize;
    Type *P1ElTy = cast<PointerType>(P1->getType())->getElementType();
    if (P1ElTy->isSized()) {
      P1Size = DL.getTypeStoreSize(P1ElTy);
    }

    uint64_t P2Size = MemoryLocation::UnknownSize;
    Type *P2ElTy = cast<PointerType>(P2->getType())->getElementType();
    if (P2ElTy->isSized()) {
      P2Size = DL.getTypeStoreSize(P2ElTy);

      return AA->alias(P1, P1Size, P2, P2Size);
    }
  }

  // Returns the closest alias between store and any of the LoadInsts
  // in toPref.
  AliasResult crossCheck(AliasAnalysis *AA, StoreInst *store, list<LoadInst *> &toPref) {
    AliasResult closest = AliasResult::NoAlias;
    Value *storePointer = store->getPointerOperand();
    for (list<LoadInst *>::iterator I = toPref.begin(), E = toPref.end();
         I != E && closest != AliasResult::MustAlias; ++I) {
      Value *loadPointer = (*I)->getPointerOperand();
      switch (pointerAlias(AA, storePointer, loadPointer,
                           (*I)->getModule()->getDataLayout())) {
      case AliasResult::NoAlias:
        break; // Already default value.
      case AliasResult::MayAlias:
        if (closest == AliasResult::NoAlias) {
          closest = AliasResult::MayAlias;
        }
        break;
      case AliasResult::PartialAlias:
        if (closest == AliasResult::NoAlias ||
            closest == AliasResult::MayAlias) {
          closest = AliasResult::PartialAlias;
        }
        break;
      case AliasResult::MustAlias:
        closest = AliasResult::MustAlias; // Highest value.
        break;
      }
    }
    return closest;
  }

  // Anotates stores in fun with the closest alias type to
  // any of the loads in toPref. (To be clear alias analysis are
  // performed between the address of each store and the address
  // of each load.) Results are annotated as metadata.
  void anotateStores(AliasAnalysis *AA, Function &fun, list<LoadInst *> &toPref) {
    list<StoreInst *> StoreList;
    findStores(fun, StoreList);
    for (list<StoreInst *>::iterator I = StoreList.begin(), E = StoreList.end();
         I != E; I++) {
      string aliasLevel;
      switch (crossCheck(AA, *I, toPref)) {
      case AliasResult::NoAlias:
        aliasLevel = "NoAlias";
        break;
      case AliasResult::MayAlias:
        aliasLevel = "MayAlias";
        break;
      case AliasResult::PartialAlias:
        aliasLevel = "PartialAlias";
        break;
      case AliasResult::MustAlias:
        aliasLevel = "MustAlias";
        break;
      }
      AttachMetadata(*I, "GlobalAlias", aliasLevel);
    }
  }
}
