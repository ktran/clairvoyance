//===-------- LoopDependency.cpp - Requirements in Loop Iteration --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file LoopDependency.cpp
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
//  Implementation of LoopDependency.h
//===----------------------------------------------------------------------===//

#include "Util/Analysis/LoopDependency.h"
#include "Util/Annotation/MetadataInfo.h"

// Set the minimum alias requirement to follow a store.
// Without flag stores are not followed at all.
static cl::opt<bool>
    FollowMay("follow-may",
              cl::desc("Require at least MayAlias to follow store"));
static cl::opt<bool>
    FollowPartial("follow-partial",
                  cl::desc("Require at least PartialAlias to follow store"));
static cl::opt<bool>
    FollowMust("follow-must", cl::desc("Require at MustAlias to follow store"));

namespace util {
  // Adds Val to Set and Q provided it is an Instruction that has
  // never before been enqued to Q. This assumes that an Instruction
  // is present in Set iff it has been added to Q.
  static void enqueueInst(Value *Val, set<Instruction *> &Set,
                   queue<Instruction *> &Q) {
    if (Instruction::classof(Val)) {
      Instruction *Inst = (Instruction *)Val;
      if (Set.insert(Inst).second) { // true if Inst was inserted
        Q.push(Inst);
      }
    }
  }

  // Enques the operands of Inst.
  static void enqueueOperands(Instruction *Inst, set<Instruction *> &Set,
                       queue<Instruction *> &Q) {
    for (User::value_op_iterator I = Inst->value_op_begin(),
                                 E = Inst->value_op_end();
         I != E; ++I) {
      enqueueInst(*I, Set, Q);
    }
  }


  // Adds all StoreInsts that could be responsible for the value read
  // by LInst to Set and Q under the same condition as in enqueueInst.
  void enqueueStores(AliasAnalysis *AA, LoadInst *LInst, set<Instruction *> &Set,
                     queue<Instruction *> &Q) {
    BasicBlock *loadBB = LInst->getParent();
    Value *Pointer = LInst->getPointerOperand();
    queue<BasicBlock *> BBQ;
    set<BasicBlock *> BBSet;
    BBQ.push(loadBB);
    bool first = true;
    bool found;
    while (!BBQ.empty()) {
      BasicBlock *BB = BBQ.front();
      BBQ.pop();
      found = false;

      BasicBlock::reverse_iterator RI(LInst->getIterator());
      for (BasicBlock::reverse_iterator iI = first ? RI : BB->rbegin(),
                                        iE = BB->rend();
           iI != iE; ++iI) {
        if (StoreInst::classof(&(*iI))) {
          StoreInst *SInst = (StoreInst *)&(*iI);
          switch (pointerAlias(AA, SInst->getPointerOperand(), Pointer,
                               iI->getModule()->getDataLayout())) {
          case AliasResult::MustAlias:
            if (FollowMust || FollowPartial || FollowMay) {
              found = true;
              enqueueInst(SInst, Set, Q);
            }
            break;
          case AliasResult::PartialAlias:
            if (FollowPartial || FollowMay) {
              enqueueInst(SInst, Set, Q);
            }
            break;
          case AliasResult::MayAlias:
            if (FollowMay) {
              enqueueInst(SInst, Set, Q);
            }
            break;
          case AliasResult::NoAlias:
            break;
          }
        } else if (Pointer == &(*iI)) {
          found = true;
        }
      }
      if (!found) {
        for (pred_iterator pI = pred_begin(BB), pE = pred_end(BB); pI != pE;
             ++pI) {
          if (BBSet.insert(*pI).second) {
            BBQ.push(*pI);
          }
        }
      }
      first = false;
    }
  }

  bool checkCalls(Instruction *I) {
    bool hasNoModifyingCalls = true;

    BasicBlock *InstBB = I->getParent();
    std::queue<BasicBlock *> BBQ;
    std::set<BasicBlock *> BBSet;

    BBQ.push(InstBB);
    // Collect all predecessor blocks
    while (!BBQ.empty()) {
      BasicBlock *BB = BBQ.front();
      BBQ.pop();
      for (pred_iterator pI = pred_begin(BB), pE = pred_end(BB); pI != pE;
           ++pI) {
        if (BBSet.insert(*pI).second) {
          BBQ.push(*pI);
        }
      }
    }

    for (Value::user_iterator U = I->user_begin(), UE = I->user_end();
         U != UE && hasNoModifyingCalls; ++U) {
      Instruction *UserInst = (Instruction *)*U;
      for (Value::user_iterator UU = UserInst->user_begin(),
                                UUE = UserInst->user_end();
           UU != UUE && hasNoModifyingCalls; ++UU) {
        if (!CallInst::classof(*UU)) {
          continue;
        }

        if (BBSet.find(((Instruction *)(*UU))->getParent()) == BBSet.end()) {
          continue;
        }

        CallInst *Call = (CallInst *)*UU;
        hasNoModifyingCalls = Call->onlyReadsMemory();

        // Allow prefetches
        if (!hasNoModifyingCalls && isa<IntrinsicInst>(Call) &&
            ((IntrinsicInst *)Call)->getIntrinsicID() == Intrinsic::prefetch) {
          hasNoModifyingCalls = true;
        }

	// Allow swoop types
	if (InstrhasMetadataKind(Call, "SwoopType")) {
	  if ("ReuseHelper" == getInstructionMD(Call, "SwoopType")) {
	    hasNoModifyingCalls = true;
	  }
	}
      }
    }

    return hasNoModifyingCalls;
  }


  void getRequirementsInIteration(AliasAnalysis *AA, LoopInfo *LI, Instruction *I, set<Instruction *> &DepSet, bool followStores) {
    set<Instruction*> DataDeps;
    getDeps(AA, LI, I, DataDeps, followStores);
    for (Instruction *DataDep : DataDeps) {
      getControlDeps(AA, LI, DataDep, DepSet);
    }
    DepSet.insert(DataDeps.begin(), DataDeps.end());
  }

  void getDeps(AliasAnalysis *AA, LoopInfo *LI, Instruction *I, set<Instruction *> &DepSet, bool followStores) {
    queue<Instruction *> Q;
    Q.push(I);

    // get enclosing loop
    const Loop *L = LI->getLoopFor(I->getParent());
    BasicBlock *H;
    
    if (L) {
      H = L->getHeader();
    }
    
    while (!Q.empty()) {
      Instruction *Inst = Q.front();
      Q.pop();

      // TODO: consider: do we want to include deps that are
      // in the entry block?
      if (L && Inst->getParent() == H && isa<PHINode>(Inst)) {
	// do not follow backedges to the head of the loop;
	// here we only consider requirements within _one_
	// iteration
	continue;
      }
    
      enqueueOperands(Inst, DepSet, Q);
      if (followStores && LoadInst::classof(Inst)) {
	enqueueStores(AA, (LoadInst *)Inst, DepSet, Q);
      }
    }
  }

  void getControlDeps(AliasAnalysis *AA, LoopInfo *LI, Instruction *I, set<Instruction *> &Deps) {
    set<BasicBlock *> Starred;
    BasicBlock *BB = I->getParent();
    std::queue<BasicBlock *> Ancestors;

    const Loop *L = LI->getLoopFor(BB);
    if (!L || BB == L->getHeader()) {
      return;
    }

    // 1. Find all ancestors of the parent block that are
    // contained in the loop.
    Starred.insert(BB);
    Ancestors.push(BB);
    while (!Ancestors.empty()) {
      BasicBlock *B = Ancestors.front();
      Ancestors.pop();

      const Loop *L = LI->getLoopFor(B);
      if (!L || B == L->getHeader()) {
        continue;
      }

      for (auto P = pred_begin(B), PE = pred_end(B); P != PE; ++P) {
        if (Starred.insert(*P).second) { // succeeded inserting it
          Ancestors.push(*P);
        }
      }
    }


    // 2. Find all terminator instructions that are crucial to the
    // execution of I
    for (auto Ancestor : Starred) {
    
      bool isMandatory = false;
      for (succ_iterator S = succ_begin(Ancestor), SE = succ_end(Ancestor); S != SE && !isMandatory; ++S) {
        if (Ancestor == BB) {
          continue;
        }
        
        // If a successor is not one of the blocks ancestor's, then this
        // terminator instruction determines whether the instruction
        // will be executed or not
        isMandatory = Starred.find(*S) == Starred.end();
      }

      if (isMandatory) {
        Deps.insert(Ancestor->getTerminator());
        getDeps(AA, LI, Ancestor->getTerminator(), Deps);
      }
    }
  }

  bool followDeps(AliasAnalysis *AA, set<Instruction *> &Set, set<Instruction *> &DepSet, bool followStores, bool followCalls) {
    bool valid = true;
    queue<Instruction *> Q;
    for (set<Instruction *>::iterator I = Set.begin(), E = Set.end();
         I != E; ++I) {
      enqueueOperands(*I, DepSet, Q);
    }
    
    while (!Q.empty()) {
      bool res = true;
      Instruction *Inst = Q.front();
      Q.pop();

      // Calls and non-local stores are prohibited.
      if (CallInst::classof(Inst)) {
        bool onlyReadsMemory = ((CallInst *)Inst)->onlyReadsMemory();
        bool annotatedToBeLocal = InstrhasMetadata(Inst, "Call", "Local");

        res = onlyReadsMemory || annotatedToBeLocal;
        if (!res) {
          errs() << " !call " << *Inst << "!>\n";
        }
      } else if (StoreInst::classof(Inst)) {
        res = isLocalPointer(((StoreInst *)Inst)->getPointerOperand());
        if (!res) {
          errs() << " <!store " << *Inst << "!>\n";
        }
      }
      if (res) {
        enqueueOperands(Inst, DepSet, Q);
        // Follow load/store
        if (followStores && LoadInst::classof(Inst)) {
          enqueueStores(AA, (LoadInst *)Inst, DepSet, Q);
        }
        if (followCalls) {
          res = checkCalls(Inst);
        }
      } else {
	valid = false;
      }
      
    }
    return valid;
  }

  bool followDeps(AliasAnalysis *AA, Instruction *Inst, set<Instruction *> &DepSet) {
    set<Instruction *> Set;
    Set.insert(Inst);
    return followDeps(AA, Set, DepSet);
  }

  void findTerminators(Function &F, set<Instruction *> &CfgSet) {
    for (Function::iterator bbI = F.begin(), bbE = F.end(); bbI != bbE; ++bbI) {
      TerminatorInst *TInst = bbI->getTerminator();
      if (TInst != NULL) {
        CfgSet.insert(TInst);
      }
    }
  }
}
