//===-------- DAEUtils.cpp - Utils for creating DAE-like loops -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file DAEUtils.cpp
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
//===----------------------------------------------------------------------===//

#include "Util/DAE/DAEUtils.h"

namespace util {
  void removeUnlisted(Function &F, set<Instruction *> &KeepSet) {
    set<Instruction *>::iterator ksI = KeepSet.begin(), ksE = KeepSet.end();
    for (inst_iterator iI = inst_begin(F), iE = inst_end(F); iI != iE;) {
      Instruction *Inst = &(*iI);
      ++iI;
      if (find(ksI, ksE, Inst) == ksE) {
        Inst->replaceAllUsesWith(UndefValue::get(Inst->getType()));
        Inst->eraseFromParent();
      }
    }
  }

  Function* cloneFunction(Function *F, ValueToValueMapTy &VMap) {
    Function *cF = Function::Create(F->getFunctionType(), F->getLinkage(),
				    F->getName() + CLONE_SUFFIX, F->getParent());
    for (Function::arg_iterator aI = F->arg_begin(), aE = F->arg_end(),
	   acI = cF->arg_begin(), acE = cF->arg_end();
	 aI != aE; ++aI, ++acI) {
      assert(acI != acE);
      acI->setName(aI->getName());
      VMap.insert(std::pair<Value *, Value *>(&*aI, &*acI));
    }
    SmallVector<ReturnInst *, 8> Returns; // Ignored
    CloneFunctionInto(cF, F, VMap, false, Returns);
    return cF;
  }

  Function* cloneFunction(Function *F) {
    ValueToValueMapTy VMap;
    return cloneFunction(F, VMap);
  }

  void replaceArgs(Function *E, Function *A) {
    Instruction *Inst;
    Value *val;
    for (inst_iterator iI = inst_begin(E), iE = inst_end(E); iI != iE; ++iI) {
      Inst = &(*iI);

      for (User::value_op_iterator uI = Inst->value_op_begin(),
	     uE = Inst->value_op_end();
	   uI != uE; ++uI) {
	val = isFunArgument(E, A, *uI);
	if (val) {
	  Inst->replaceUsesOfWith(*uI, val);
	}
      }
    }
  }

    // Inserts a prefetch for LInst as early as possible
  // (i.e. as soon as the adress has been computed).
  // The prefetch and all its dependencies will also
  // be inserted in toKeep.
  // Returns the result of the insertion.
  PrefInsertResult
  insertPrefetch(AliasAnalysis *AA, LoadInst *LInst, set<Instruction *> &toKeep,
                 map<LoadInst *, pair<CastInst *, CallInst *>> &prefs,
		 unsigned threshold) {

    // Follow dependencies
    set<Instruction *> Deps;
    if (followDeps(AA, LInst, Deps)) {
      if (isUnderThreshold(Deps, threshold)) {
        toKeep.insert(Deps.begin(), Deps.end());
      } else {
        return IndirLimit;
      }
    } else {
      return BadDeps;
    }

    // Extract usefull information
    bool prefetchExists = false;
    Value *DataPtr = LInst->getPointerOperand();
    BasicBlock *BB = LInst->getParent();
    BasicBlock *EntryBlock =
        &(LInst->getParent()->getParent()->getEntryBlock());
    for (map<LoadInst *, pair<CastInst *, CallInst *>>::iterator
             I = prefs.begin(),
             E = prefs.end();
         I != E; ++I) {
      LoadInst *LD = I->first;
      if (LD->getPointerOperand() == DataPtr) {
        // Might also be nullptr
        BasicBlock *LDBB = LD->getParent();
        if (BB == EntryBlock && LDBB == EntryBlock ||
            BB != EntryBlock && LDBB != EntryBlock) {
          prefetchExists = true;
          break;
        }
      }
    }

    if (prefetchExists) {
      return Redundant;
    }

    unsigned PtrAS = LInst->getPointerAddressSpace();
    LLVMContext &Context = DataPtr->getContext();

    // Make sure type is correct
    Instruction *InsertPoint = LInst;
    Type *I8Ptr = Type::getInt8PtrTy(Context, PtrAS);
    CastInst *Cast =
        CastInst::CreatePointerCast(DataPtr, I8Ptr, "", InsertPoint);

    // Insert prefetch
    IRBuilder<> Builder(InsertPoint);
    Module *M = LInst->getParent()->getParent()->getParent();
    Type *I32 = Type::getInt32Ty(LInst->getContext());
    Value *PrefFun = Intrinsic::getDeclaration(M, Intrinsic::prefetch);
    CallInst *Prefetch = Builder.CreateCall(
        PrefFun, {Cast, ConstantInt::get(I32, 0),                       // read
                  ConstantInt::get(I32, 3), ConstantInt::get(I32, 1)}); // data

    // Inset prefetch instructions into book keeping
    toKeep.insert(Cast);
    toKeep.insert(Prefetch);
    prefs.insert(make_pair(LInst, make_pair(Cast, Prefetch)));

    return Inserted;
  }

  void findLoads(Function &F, list<LoadInst *> &LoadList) {
    for (inst_iterator iI = inst_begin(F), iE = inst_end(F); iI != iE; ++iI) {
      if (LoadInst::classof(&(*iI))) {
        LoadList.push_back((LoadInst *)&(*iI));
      }
    }
  }

  void findVisibleLoads(list<LoadInst *> &LoadList, list<LoadInst *> &VisList) {
    for (list<LoadInst *>::iterator I = LoadList.begin(), E = LoadList.end();
         I != E; ++I) {
      if (isNonLocalPointer((*I)->getPointerOperand())) {
        VisList.push_back(*I);
      }
    }
  }

  bool isUnderThreshold(set<Instruction *> Deps, unsigned Threshold) {
    unsigned count = 0;
    for (set<Instruction *>::iterator dI = Deps.begin(), dE = Deps.end();
         dI != dE && count <= Threshold; ++dI) {
      if (LoadInst::classof(*dI)) {
        ++count;
      }
    }
    return count <= Threshold;
  }

  Value* isFunArgument(Function *E, Function *A, Value *arg) {
    for (Function::arg_iterator aI = E->arg_begin(), aE = E->arg_end(),
	   acI = A->arg_begin();
	 aI != aE; ++aI, ++acI) {
      if ((Value *)arg == &*aI)
	return &*acI;
    }
    return 0;
  }

  bool isNonLocalPointer(Value *Pointer) { return !isLocalPointer(Pointer); }

  bool isLocalPointer(Value *Pointer) {
    if (!Instruction::classof(Pointer)) {
      return false;
    }
    Instruction *PtrInst = (Instruction *)Pointer;
    if (AllocaInst::classof(Pointer)) {
      // A locally defined memory location
      return true;
    }
    unsigned poi;
    if (GetElementPtrInst::classof(Pointer)) {
      poi = GetElementPtrInst::getPointerOperandIndex();
    } else if (CastInst::classof(Pointer)) {
      poi = 0; // The only operand
    } else if (LoadInst::classof(Pointer)) {
      // Assumes that global pointers are never stored in local
      // structures. Otherwise this could produce false positives.
      poi = LoadInst::getPointerOperandIndex();
    } else {
      return false;
    }
    Value *Pointer2 = PtrInst->getOperand(poi);
    return isLocalPointer(Pointer2);
  }


}

