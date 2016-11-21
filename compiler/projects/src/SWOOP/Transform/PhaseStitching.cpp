//===--------------- PhaseStitching.cpp - Utils to stitch Access/Execute Phases
//---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file PhaseStitching.cpp
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
//
//===----------------------------------------------------------------------===//

#include "PhaseStitching.h"

#include <list>
#include <queue>
#include <stack>

#include "llvm/Analysis/AliasAnalysis.h"

#include "llvm/Transforms/Utils/Cloning.h"

#include "llvm/Analysis/Interval.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#define PRINTSTREAM errs() // raw_ostream

using namespace llvm;
using namespace std;

static std::string ASSEMBLY_SIDE_EFFECT_CONSTRAINT =
    "~{dirflag},~{fpsr},~{flags},~{memory}";

BasicBlock *getExecuteRoot(Function *F);
BasicBlock *getExecuteLatch(BasicBlock *executeRoot, BasicBlock *executeBody);

Value *findInsertionPoint(DominatorTree *DT, BasicBlock *Use, Instruction *Def,
                          set<BasicBlock *> &RelevantBlocks,
                          map<BasicBlock *, Value *> &ValToUseCache);
void collectPhiNodes(BasicBlock *bb, vector<PHINode *> &bbPN);
void insertMissingPhiForAccess(BasicBlock *accessBody, BasicBlock *executeBody,
                               BasicBlock *accessRoot, BasicBlock *executeRoot);
void insertMissingIncomingForOriginal(BasicBlock *accessBody, BasicBlock *executeBody,
				      BasicBlock *accessRoot,
				      BasicBlock *executeRoot,
				      ValueToValueMapTy &VMapRev);
void replaceDuplicatePhiNodes(Loop *L);

BasicBlock *getExitingBlock(BasicBlock *latch);

void insertMissingPhiNodesForDomination(Loop *L, DominatorTree *DT,
                                        vector<BasicBlock *> &executeBlocks,
                                        BasicBlock *root);
bool simplifyLoopLatch(Loop *L, LoopInfo *LI, DominatorTree *DT);

void insertInlineAssembly(LLVMContext &context, std::string &asmString,
                          Instruction *insertBeforeInstruction,
                          std::string &constraints);
void makeLabel(string prefix, string stage, string *label, int phaseCount);
void replaceSuccessor(BasicBlock *B, BasicBlock *toReplace, BasicBlock *toUse);

struct equivalentPhiNode {
public:
  equivalentPhiNode(const PHINode *wantedPN) : wanted(wantedPN) {}

  bool operator()(const PHINode *PN) {
    if (wanted->getType() != PN->getType()) {
      return false;
    }

    
    bool SameRootVal = false;
    for (int i = 0; i < wanted->getNumIncomingValues(); ++i) {
      for (int j = 0; j < PN->getNumIncomingValues(); ++j) {
	if (wanted->getIncomingValue(i) == PN->getIncomingValue(j)) {
	  SameRootVal = true;
	}
	if (wanted->getIncomingBlock(i) == PN->getIncomingBlock(j)) {
	  return false;
	}
      }
    }

    return SameRootVal;
  }

private:
  const PHINode *wanted;
};

struct samePhiNode {
public:
  samePhiNode(const PHINode *wantedPN) : wanted(wantedPN) {}

  bool operator()(const PHINode *PN) {
    bool sameType = wanted->getType() == PN->getType();
    bool sameNumIncoming =
        wanted->getNumIncomingValues() == PN->getNumIncomingValues();

    if (!sameType || !sameNumIncoming) {
      return false;
    }

    for (int i = 0; i < wanted->getNumIncomingValues(); ++i) {
      bool sameIncomingBlock =
          wanted->getIncomingBlock(i) == PN->getIncomingBlock(i);
      bool sameIncomingValue =
          wanted->getIncomingValue(i) == PN->getIncomingValue(i);

      if (!sameIncomingValue || !sameIncomingValue) {
        return false;
      }
    }

    return true;
  }

private:
  const PHINode *wanted;
};

void replaceSuccessor(BasicBlock *B, BasicBlock *toReplace, BasicBlock *toUse) {
  BranchInst *EBI = dyn_cast<BranchInst>(B->getTerminator());
  int indexOfSuccessorToReplace = 0;

  while (EBI->getSuccessor(indexOfSuccessorToReplace) != toReplace) {
    ++indexOfSuccessorToReplace;
  }
  EBI->setSuccessor(indexOfSuccessorToReplace, toUse);
}

void makeLabel(string prefix, string stage, string *label, int phaseCount) {
  *label = to_string(phaseCount) + ":";
}

BasicBlock *getExitingBlock(BasicBlock *latch) {
  BasicBlock *executeBodyEnd;
  TerminatorInst *TI = latch->getTerminator();

  if (TI->getNumSuccessors() == 1) {
    executeBodyEnd = latch->getSinglePredecessor();
  } else {
    executeBodyEnd = latch;
  }
  return executeBodyEnd;
}

void insertInlineAssembly(LLVMContext &context, string &asmString,
                          Instruction *insertBeforeInstruction,
                          string &constraints) {
  FunctionType *AsmFTy = FunctionType::get(Type::getVoidTy(context), false);
  InlineAsm *IA = InlineAsm::get(AsmFTy, asmString, constraints, true, false);
  vector<Value *> AsmArgs;
  CallInst::Create(IA, Twine(""), insertBeforeInstruction);
}

void gatherSuccessorsWithinLoop(BasicBlock *B, set<BasicBlock *> &Succs,
                                Loop *L) {
  std::queue<BasicBlock *> ToVisit;

  ToVisit.push(B);
  Succs.insert(B);

  while (!ToVisit.empty()) {
    BasicBlock *Succ = ToVisit.front();
    ToVisit.pop();

    for (auto S = succ_begin(Succ), SE = succ_end(Succ); S != SE; ++S) {
      // Do not take back edges when adding successors
      if ((*S) != L->getHeader() && Succs.insert(*S).second) {
        ToVisit.push(*S);
      }
    }
  }
}

void insertMissingPhiNodesForDomination(
    Loop *L, DominatorTree *DT, vector<BasicBlock *> &blocksMissingPhiNodes,
    BasicBlock *root) {
  DT->recalculate(*(L->getHeader()->getParent()));

  for (auto BI = blocksMissingPhiNodes.begin(),
            BE = blocksMissingPhiNodes.end();
       BI != BE; ++BI) {
    BasicBlock *bb = *BI;

    for (BasicBlock::iterator II = bb->begin(), IE = bb->end(); II != IE;
         ++II) {
      Instruction *I = &*II;
      PHINode *PN = dyn_cast<PHINode>(I);
      for (User::op_iterator OI = I->op_begin(), OE = I->op_end(); OI != OE;
           ++OI) {

        if (*OI == nullptr) {
          continue;
        }

        if (Instruction *VI = dyn_cast<Instruction>(*OI)) {
          BasicBlock *ToDominate = bb;
          if (PN) {
            // In case of Phi Nodes, the definition needs to
            // dominate the incoming block instead
            ToDominate = PN->getIncomingBlock(*OI);
          }
          if (VI->getParent() != ToDominate && !DT->dominates(VI, ToDominate)) {
            map<BasicBlock *, Value *> ValToUseCache;
            std::set<BasicBlock *> Succs;
            gatherSuccessorsWithinLoop(VI->getParent(), Succs, L);
            Value *instructionToUse =
                findInsertionPoint(DT, root, VI, Succs, ValToUseCache);
            I->replaceUsesOfWith(*OI, instructionToUse);
          }
        }
      }
    }
  }
}

void replaceDuplicatePhiNodes(Loop *L) {
  for (auto BI = L->block_begin(), BE = L->block_end(); BI != BE; ++BI) {
    vector<PHINode *> uniquePhiNodes;
    vector<PHINode *> toDelete;

    for (BasicBlock::iterator aI = (*BI)->begin(); isa<PHINode>(aI); ++aI) {
      PHINode *aPN = dyn_cast<PHINode>(&*aI);

      auto foundPN = find_if(uniquePhiNodes.begin(), uniquePhiNodes.end(),
                             samePhiNode(aPN));
      if (foundPN == uniquePhiNodes.end()) {

        // If a phi node is unique, keep it
        uniquePhiNodes.push_back(aPN);
      } else {

        // Otherwise, replace all uses of this phi node with the duplicate
        aPN->replaceAllUsesWith(*foundPN);
        toDelete.push_back(aPN);
      }
    }

    // Remove all duplicate phi nodes
    for (auto PNI = toDelete.begin(), PNE = toDelete.end(); PNI != PNE; ++PNI) {
      (*PNI)->eraseFromParent();
    }
  }
}

bool simplifyLoopLatch(Loop *L, LoopInfo *LI, DominatorTree *DT) {
  BasicBlock *Latch = L->getLoopLatch();
  if (!Latch || Latch->hasAddressTaken())
    return false;

  BranchInst *Jmp = dyn_cast<BranchInst>(Latch->getTerminator());
  if (!Jmp || !Jmp->isUnconditional())
    return false;

  BasicBlock *LastExit = Latch->getSinglePredecessor();
  if (!LastExit || !L->isLoopExiting(LastExit))
    return false;

  BranchInst *BI = dyn_cast<BranchInst>(LastExit->getTerminator());
  if (!BI)
    return false;

  // Hoist the instructions from Latch into LastExit.
  LastExit->getInstList().splice(BI->getIterator(), Latch->getInstList(),
                                 Latch->begin(), Jmp->getIterator());

  unsigned FallThruPath = BI->getSuccessor(0) == Latch ? 0 : 1;
  BasicBlock *Header = Jmp->getSuccessor(0);
  assert(Header == L->getHeader() && "expected a backward branch");

  // Remove Latch from the CFG so that LastExit becomes the new Latch.
  BI->setSuccessor(FallThruPath, Header);
  Latch->replaceSuccessorsPhiUsesWith(LastExit);
  Jmp->eraseFromParent();

  // Nuke the Latch block.
  assert(Latch->empty() && "unable to evacuate Latch");
  LI->removeBlock(Latch);
  DT->eraseNode(Latch);
  Latch->eraseFromParent();
  return true;
}

void insertMissingIncomingForOriginal(BasicBlock *accessBody, BasicBlock *executeBody,
				      BasicBlock *accessRoot,
				      BasicBlock *executeRoot,
				      ValueToValueMapTy &VMapRev) {
  vector<PHINode *> executePN;
  collectPhiNodes(executeBody, executePN);

  vector<PHINode *> accessPN;
  collectPhiNodes(accessBody, accessPN);


  for (vector<PHINode *>::iterator P = executePN.begin(), PE = executePN.end();
       P != PE; ++P) {
    PHINode *PN = *P;
    Value *MappedValue = VMapRev[PN];
    
    if (MappedValue) {
      PHINode *AccessEquivPN = dyn_cast<PHINode>(MappedValue);
      for (int i = 0; i < PN->getNumIncomingValues(); ++i) {
	BasicBlock *IncomingBlock = PN->getIncomingBlock(i);
	if (IncomingBlock != executeRoot) {
	  AccessEquivPN->addIncoming(PN->getIncomingValue(i), IncomingBlock);
	}
      }
      PN->replaceAllUsesWith(AccessEquivPN);
      PN->eraseFromParent();
    }
  }
}

void collectPhiNodes(BasicBlock *bb, vector<PHINode *> &bbPN) {
  for (BasicBlock::iterator aI = bb->begin(); isa<PHINode>(aI); ++aI) {
    PHINode *aPN = dyn_cast<PHINode>(&*aI);
    bbPN.push_back(aPN);
  }
}

Value *findInsertionPoint(DominatorTree *DT, BasicBlock *UseBB,
                          Instruction *Def, set<BasicBlock *> &RelevantBlocks,
                          map<BasicBlock *, Value *> &ValToUseCache) {
  BasicBlock *Use = UseBB;
  bool firstRun = true;
  do {
    if (!firstRun) {
      Use = Use->getSinglePredecessor();
    }

    if (ValToUseCache.find(Use) != ValToUseCache.end()) {
      return ValToUseCache[Use];
    }

    if (RelevantBlocks.find(Use) == RelevantBlocks.end()) {
      Value *ToUse = UndefValue::get(Def->getType());
      ValToUseCache[Use] = ToUse;
      return ToUse;
    }

    if (Def->getParent() == Use || DT->dominates(Def, Use)) {
      ValToUseCache[Use] = Def;
      return Def;
    }

    // Unnecessary (?): because it won't be in RelevantBlocks
    // if (DT->dominates(Use, Def->getParent())) {
    //   // No definition could be found for this path
    //   return UndefValue::get(Def->getType());
    // }

    if (pred_begin(Use) == pred_end(Use)) {
      // Nowhere to search anymore.
      Value *ToUse = UndefValue::get(Def->getType());
      ValToUseCache[Use] = ToUse;
      return ToUse;
    }

    firstRun = false;
  } while (Use->getSinglePredecessor());

  // Otherwise, a PHI might be necessary
  list<pair<BasicBlock *, Value *>> IncomingVals;
  bool allIncomingAreEqual = true;
  Value *Incoming;

  auto PI = pred_begin(Use), PE = pred_end(Use);
  Incoming = findInsertionPoint(DT, *PI, Def, RelevantBlocks, ValToUseCache);
  IncomingVals.push_back(make_pair(*PI, Incoming));
  ++PI;

  while (PI != PE) {
    Value *IncomingForPred =
        findInsertionPoint(DT, *PI, Def, RelevantBlocks, ValToUseCache);
    IncomingVals.push_back(make_pair(*PI, IncomingForPred));
    if (IncomingForPred != Incoming) {
      allIncomingAreEqual = false;
    }
    ++PI;
  }

  Value *ToUse;
  if (allIncomingAreEqual) {
    ToUse = Incoming;
  } else {
    PHINode *newPHI =
        PHINode::Create(Def->getType(), 0, Twine(Def->getName().str() + ".phi"),
                        &(Use->front()));
    for (auto V = IncomingVals.begin(), VE = IncomingVals.end(); V != VE; ++V) {
      newPHI->addIncoming((*V).second, (*V).first);
    }
    ToUse = newPHI;
  }

  ValToUseCache[Use] = ToUse;
  return ToUse;
}

BasicBlock *getExecuteLatch(BasicBlock *executeRoot, BasicBlock *executeBody) {
  BasicBlock *executeLatch;
  for (auto it = pred_begin(executeBody); it != pred_end(executeBody); ++it) {
    BasicBlock *b = *it;
    if (b != executeRoot) {
      executeLatch = b;
      break;
    }
  }
  return executeLatch;
}

BasicBlock *getExecuteRoot(Function *F) {
  BasicBlock *executeRoot = nullptr;
  for (Function::iterator i = F->begin(), e = F->end(); i != e; ++i) {
    if (i->hasNUses(0) && &*i != &(F->getEntryBlock())) {
      executeRoot = &*i;
      break;
    }
  }
  return executeRoot;
}

// bool toAdaptForOracle(Loop *L) {
//   Function *F = L->getHeader()->getParent();
//   if (F->getName().str().find(F_KERNEL_SUBSTR) != std::string::npos &&
//           L->getLoopPreheader() && &(F->getEntryBlock()) ==
//           L->getLoopPreheader()) {
//     return true;
//   }
//   return false;
// }

Loop *getLoop(Function &F, LoopInfo &LI) {
  return LI.getLoopFor(F.getEntryBlock().getTerminator()->getSuccessor(0));
}

bool stitch(Function &F, Function &ToAppend, ValueToValueMapTy &VMap, ValueToValueMapTy &VMapRev, LoopInfo &LI, DominatorTree &DT,
            bool forceIncrement, string type, int phaseCount) {
  Loop *L = getLoop(F, LI);
  BasicBlock *accessLatch = L->getLoopLatch();
  BasicBlock *accessExit = L->getUniqueExitBlock();
  BasicBlock *accessBody = L->getHeader();
  BasicBlock *accessRoot = L->getLoopPredecessor();

  BasicBlock *executeRoot = &(ToAppend.getEntryBlock());
  TerminatorInst *executeRootEnd = executeRoot->getTerminator();
  if (executeRootEnd->getNumSuccessors() != 1) {
    errs() << "Expected unconditional jump in execute root.\n";
    return false;
  }

  BasicBlock *executeBody = executeRootEnd->getSuccessor(0);
  BasicBlock *executeLatch = getExecuteLatch(executeRoot, executeBody);
  BasicBlock *executeBodyEnd = getExitingBlock(executeLatch);
  BasicBlock *executeExit;

  F.getBasicBlockList().splice(F.end(), ToAppend.getBasicBlockList());
  ToAppend.removeFromParent();

  // Find exiting block
  BasicBlock *accessBodyEnd = NULL;
  SmallVector<BasicBlock *, 20> AccessExitingBlocks;
  L->getExitingBlocks(AccessExitingBlocks);

  // Gather all execute blocks
  vector<BasicBlock *> executeBlocks;

  for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
    BasicBlock *BB = &*i;
    if (!L->contains(BB) && BB != accessExit &&
        BB != accessRoot) { // Do not re-add access blocks
      if (isa<ReturnInst>(BB->getTerminator())) {

        // do not add to loop - it's already part of the function
        executeExit = BB;
      } else {
        L->addBasicBlockToLoop(BB, LI);
        executeBlocks.push_back(BB);
      }
    }
  }

  // Remove jump from execute latch to execute body
  replaceSuccessor(executeLatch, executeBody, accessBody);

  if (AccessExitingBlocks.size() == 1) {
    accessBodyEnd = *(AccessExitingBlocks.begin());
  } else {
    // We assume: if several exiting blocks exist, the loop was unrolled
    for (SmallVectorImpl<BasicBlock *>::iterator
             BB = AccessExitingBlocks.begin(),
             BE = AccessExitingBlocks.end();
         BB != BE; ++BB) {
      BasicBlock *B = *BB;
      TerminatorInst *TI = B->getTerminator();
      bool isIntermediate = false;
      for (int i = 0; i < TI->getNumSuccessors(); ++i) {
        BasicBlock *S = TI->getSuccessor(i);
        if (S != accessBody && S != accessExit) {
          isIntermediate = true;
        }
      }

      if (!isIntermediate) {
        // there should only be one
        accessBodyEnd = B;
      }
    }
  }

  // Remove jump to access exit/access latch
  TerminatorInst *TI = accessLatch->getTerminator();
  TI->eraseFromParent();

  if (forceIncrement) {
    // for now, this is hw-swoop
    IRBuilder<> builder(accessLatch);
    FunctionType *AsmFTy =
        FunctionType::get(Type::getInt1Ty(accessBodyEnd->getContext()), false);
    std::string condition = "=r" + ASSEMBLY_SIDE_EFFECT_CONSTRAINT;
    InlineAsm *IA =
        InlineAsm::get(AsmFTy, "movb $$0, $0", condition, true, false);

    vector<Value *> AsmArgs;
    CallInst *callInst = CallInst::Create(IA, Twine("ckmiss"), accessLatch);

    BranchInst::Create(accessBody, accessExit, callInst, accessLatch);
  } else {
    BranchInst::Create(accessExit, accessLatch);
  }

  Instruction *AccessReturnI = accessExit->getTerminator();
  BranchInst *AccessExitBI = BranchInst::Create(executeRoot);
  ReplaceInstWithInst(AccessReturnI, AccessExitBI);
  L->addBasicBlockToLoop(accessExit, LI);

  // Replace phi nodes in execute phase by values that should be used
  vector<BasicBlock *> accessReplaceWith, executeReplace;
  accessReplaceWith.push_back(accessBody);
  executeReplace.push_back(executeBody);
  
  insertMissingIncomingForOriginal(accessBody, executeBody, accessRoot, executeRoot, VMapRev);

  // For all other values that are already computed in the access phase to
  // be transferred from execute -> access, insert a phi value
  for (BasicBlock::iterator aI = (accessBody)->begin(); isa<PHINode>(aI);
       ++aI) {
    PHINode *PN = dyn_cast<PHINode>(&*aI);

    if (PN->getBasicBlockIndex(accessLatch) != -1 &&
        PN->getBasicBlockIndex(executeLatch) == -1) {
      Value *AccessPhiVal = PN->getIncomingValueForBlock(accessLatch);
      PN->addIncoming(AccessPhiVal, executeLatch);

      if (!forceIncrement) {
        PN->removeIncomingValue(accessLatch);
      }
    }
    
    Value *Undef = UndefValue::get(PN->getType());
    for (int i = 0; i < PN->getNumIncomingValues(); ++i) {
      // Remove all incoming values that are undef
      if (PN->getIncomingValue(i) == Undef) {
	PN->removeIncomingValue(i);
      }
    }
  }

  // clean up phi nodes after removing the jump from the latch
  vector<PHINode *> accessPN;
  collectPhiNodes(accessBody, accessPN);

  for (vector<PHINode *>::iterator P = accessPN.begin(), PE = accessPN.end();
       P != PE; ++P) {
    PHINode *PN = *P;
    int LatchIndex = PN->getBasicBlockIndex(accessLatch);
    if (LatchIndex >= 0) {
      PN->removeIncomingValue(LatchIndex);
    }
  }

  // Insert inline assembly labels and separator between access and execute
  std::string prefix = F.getName().str() + "_";
  std::string accessLabel, accessEndLabel;
  std::string executeLabel, executeEndLabel;

  makeLabel(prefix, type, &executeLabel, phaseCount);
  makeLabel(prefix, type + "_end", &executeEndLabel, phaseCount);
  LLVMContext &context = F.getContext();

  insertInlineAssembly(context, executeLabel, &*(executeRoot->begin()),
                       ASSEMBLY_SIDE_EFFECT_CONSTRAINT);
  insertInlineAssembly(context, executeEndLabel,
                       executeBodyEnd->getTerminator(),
                       ASSEMBLY_SIDE_EFFECT_CONSTRAINT);

  // Now that we're done with combining access + execute, make sure that
  // all added basic blocks to the Loop are actually part of the loop..
  DT.recalculate(*(L->getHeader()->getParent()));

  for (auto P = pred_begin(executeExit), PE = pred_end(executeExit); P != PE;
       ++P) {
    BasicBlock *Pred = *P;
    if (!DT.dominates(Pred, executeExit) && L->contains(Pred)) {
      vector<BasicBlock *> MissingPNBlock;
      MissingPNBlock.push_back(Pred);
      insertMissingPhiNodesForDomination(L, &DT, MissingPNBlock, executeRoot);

      L->removeBlockFromLoop(Pred);
    }
  }

  return true;
}

bool stitchAEDecision(Function &F, Function &Optimized, ValueToValueMapTy &VMapRev, AllocaInst *branch_cond,
		      BasicBlock *DecisionBlock,
		      LoopInfo &LI, DominatorTree &DT,
		      string type, int phaseCount) {
  Loop *L = getLoop(F, LI);
  BasicBlock *accessLatch = L->getLoopLatch();
  BasicBlock *accessExit = L->getUniqueExitBlock();
  BasicBlock *accessBody = L->getHeader();
  BasicBlock *accessRoot = L->getLoopPredecessor();

  BasicBlock *executeRoot = &(Optimized.getEntryBlock());
  TerminatorInst *executeRootEnd = executeRoot->getTerminator();
  if (executeRootEnd->getNumSuccessors() != 1) {
    errs() << "Expected unconditional jump in execute root.\n";
    return false;
  }

  BasicBlock *executeBody = executeRootEnd->getSuccessor(0);
  BasicBlock *executeLatch = getExecuteLatch(executeRoot, executeBody);
  BasicBlock *executeBodyEnd = getExitingBlock(executeLatch);
  BasicBlock *executeExit;

  F.getBasicBlockList().splice(F.end(), Optimized.getBasicBlockList());
  Optimized.removeFromParent();

  // Find exiting block
  BasicBlock *accessBodyEnd = NULL;
  SmallVector<BasicBlock *, 20> AccessExitingBlocks;
  L->getExitingBlocks(AccessExitingBlocks);

  // Gather all execute blocks
  vector<BasicBlock *> executeBlocks;

  for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
    BasicBlock *BB = &*i;
    if (!L->contains(BB) && BB != accessExit &&
        BB != accessRoot) { // Do not re-add access blocks
      if (isa<ReturnInst>(BB->getTerminator())) {

        // do not add to loop - it's already part of the function
        executeExit = BB;
      } else {
        L->addBasicBlockToLoop(BB, LI);
        executeBlocks.push_back(BB);
      }
    }
  }

  // Remove jump from execute latch to execute body
  replaceSuccessor(executeLatch, executeBody, accessBody);

  if (AccessExitingBlocks.size() == 1) {
    accessBodyEnd = *(AccessExitingBlocks.begin());
  } else {
    // We assume: if several exiting blocks exist, the loop was unrolled
    for (SmallVectorImpl<BasicBlock *>::iterator
             BB = AccessExitingBlocks.begin(),
             BE = AccessExitingBlocks.end();
         BB != BE; ++BB) {
      BasicBlock *B = *BB;
      TerminatorInst *TI = B->getTerminator();
      bool isIntermediate = false;
      for (int i = 0; i < TI->getNumSuccessors(); ++i) {
        BasicBlock *S = TI->getSuccessor(i);
        if (S != accessBody && S != accessExit) {
          isIntermediate = true;
        }
      }

      if (!isIntermediate) {
        // there should only be one
        accessBodyEnd = B;
      }
    }
  }

  TerminatorInst *DecisionBlockTI = DecisionBlock->getTerminator();
  IRBuilder<> Builder(DecisionBlockTI);
  BasicBlock *OptimizedAccessBB = DecisionBlockTI->getSuccessor(0);
  LoadInst *branch_value = Builder.CreateLoad(branch_cond);
  Builder.CreateCondBr(branch_value, OptimizedAccessBB, executeRoot);
  DecisionBlockTI->eraseFromParent();
	
  // Replace phi nodes in execute phase by values that should be used
  vector<BasicBlock *> accessReplaceWith, executeReplace;
  accessReplaceWith.push_back(accessBody);
  executeReplace.push_back(executeBody);

  insertMissingIncomingForOriginal(accessBody, executeBody, accessRoot, executeRoot, VMapRev);
   
  // For all other values that are already computed in the access phase to
  // be transferred from execute -> access, insert a phi value
  for (BasicBlock::iterator aI = (accessBody)->begin(); isa<PHINode>(aI);
       ++aI) {
    PHINode *PN = dyn_cast<PHINode>(&*aI);

    if (PN->getBasicBlockIndex(accessLatch) != -1 &&
        PN->getBasicBlockIndex(executeLatch) == -1) {
      Value *AccessPhiVal = PN->getIncomingValueForBlock(accessLatch);
      PN->addIncoming(AccessPhiVal, executeLatch);

      //PN->removeIncomingValue(accessLatch);
    }

    Value *Undef = UndefValue::get(PN->getType());
    for (int i = 0; i < PN->getNumIncomingValues(); ++i) {
      // Remove all incoming values that are undef
      if (PN->getIncomingValue(i) == Undef) {
	PN->removeIncomingValue(i);
      }
    }
  }

  
  

  // Insert inline assembly labels and separator between access and execute
  std::string prefix = F.getName().str() + "_";
  std::string accessLabel, accessEndLabel;
  std::string executeLabel, executeEndLabel;

  makeLabel(prefix, type, &executeLabel, phaseCount);
  makeLabel(prefix, type + "_end", &executeEndLabel, phaseCount);
  LLVMContext &context = F.getContext();

  insertInlineAssembly(context, executeLabel, &*(executeRoot->begin()),
                       ASSEMBLY_SIDE_EFFECT_CONSTRAINT);
  insertInlineAssembly(context, executeEndLabel,
                       executeBodyEnd->getTerminator(),
                       ASSEMBLY_SIDE_EFFECT_CONSTRAINT);

  // Now that we're done with combining access + execute, make sure that
  // all added basic blocks to the Loop are actually part of the loop..
  DT.recalculate(*(L->getHeader()->getParent()));

  for (auto P = pred_begin(executeExit), PE = pred_end(executeExit); P != PE;
       ++P) {
    BasicBlock *Pred = *P;
    if (!DT.dominates(Pred, executeExit) && L->contains(Pred)) {
      vector<BasicBlock *> MissingPNBlock;
      MissingPNBlock.push_back(Pred);
      insertMissingPhiNodesForDomination(L, &DT, MissingPNBlock, executeRoot);

      L->removeBlockFromLoop(Pred);
    }
  }

  return true;
}

void ensureStrictSSA(Function &F, LoopInfo &LI, DominatorTree &DT,
                     vector<BasicBlock *> &PhaseRoots) {
  // Insert lacking phi nodes: if a value defined during the access phase
  // is used in the execute phase, but its definition does not
  // dominate all paths to
  // that usage, phi nodes with undefined values need to be introduced.
  // This is acceptable,  as we assume here that a value is only used,
  // if the path including its definition was taken.
  Loop *L = getLoop(F, LI);

  int numberOfPhases = PhaseRoots.size();
  for (int i = 0; i < numberOfPhases; ++i) {
    set<BasicBlock *> PotentiallyMissingPN;
    queue<BasicBlock *> BBQ;

    
    BBQ.push(PhaseRoots[i]);
    PotentiallyMissingPN.insert(PhaseRoots[i]);
    while (!BBQ.empty()) {
      BasicBlock *B = BBQ.front();
      BBQ.pop();
      if (!LI.getLoopFor(B) || (i < numberOfPhases - 1) && B == PhaseRoots[i + 1]) {
         continue;
      }

      for (auto S = succ_begin(B), SE = succ_end(B); S != SE; ++S) {
        if (PotentiallyMissingPN.insert(*S).second && (*S) != L->getHeader()) {
          BBQ.push(*S);
        }
      }
    }

    vector<BasicBlock *> BlocksToProcess = vector<BasicBlock *>(
        PotentiallyMissingPN.begin(), PotentiallyMissingPN.end());
    insertMissingPhiNodesForDomination(L, &DT, BlocksToProcess, PhaseRoots[i]);
  }

  // replace phi nodes with the same incoming values,
  // that were added due to the previous step
  replaceDuplicatePhiNodes(L);
}
