//===------- BasicSwoop.h - basic and conservative swoop ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file BasicSwoop.h
///
/// \brief Basic and conservative Swoop version.
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// Basic swoop: conservatively hoisting & reusing loads.
//
//===----------------------------------------------------------------------===//

#ifndef SWOOP_BASICSWOOP_H
#define SWOOP_BASICSWOOP_H

#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <string>

#include "Util/Analysis/LoopCarriedDependencyAnalysis.h"
#include "Util/Annotation/MetadataInfo.h"
#include "Util/DAE/DAEUtils.h"
#include "Util/Analysis/LoopDependency.h"

#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/IR/IntrinsicInst.h>
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

using namespace llvm;
using namespace std;
using namespace util;

namespace swoop {

  // Helper struct: represents a phase (in multi-access)
  struct Phase {
  public:
    // The function clone
    Function *F;

    // The mapping between the cloned function and F (and backwards)
    ValueToValueMapTy VMap;
    ValueToValueMapTy VMapRev;

    // Keeps track of which loads in F should be reused (A: load, E: use),
    // prefetched (A: prefetch, E: load), or just loaded (A: load, E: load).
    list<LoadInst *> ToReuse;
    list<LoadInst *> ToPref;
    list<LoadInst *> ToLoad;
  };
  
  struct SwoopDAE : public ModulePass{
    static char ID;
  SwoopDAE() : ModulePass(ID) {}

  public:
    virtual bool runOnModule(Module &M);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const override;

    // Main functionality: swoopifying function F
    bool swoopify(Function &F);

  protected:
    LoopInfo *LI;

    // Alias Analysis
    AliasAnalysis *AA;

    // Dominator Tree
    DominatorTree *DT;

    // Postdom Tree
    PostDominatorTree *PDT;

    // Divide loads into each category: prefetch, reuse or load
    virtual void divideLoads(list<LoadInst *> &toHoist,
                             list<LoadInst *> &toPref,
                             list<LoadInst *> &toReuse,
                             list<LoadInst *> &toLoad,
                             unsigned int UnrollCount);

    // Returns true iff F is a swoop kernel.
    virtual bool isSwoopKernel(Function &F);

    // Returns which instructions to reuse in terms of lcd
    virtual LCDResult acceptedForReuse();

    // Determines which instructions to reuse in the execute phase
    virtual void selectInstructionsToReuseInExecute(Function *F, set<Instruction*> *toKeep, set<Instruction*> *toUpdateForNextPhases, LCDResult MinLCDRequirement, bool ReuseAll, bool ReuseBranchCondition);

    ////////
    // Filtering on Loop-carried dependencies
    ////////
    virtual void filterLoadsOnLCD(AliasAnalysis *AA,
                                  LoopInfo *LI,
                                  list<LoadInst *> &Loads,
                                  list<LoadInst *> &FilteredLoads,
                                  unsigned int UnrollCount);

  private:

    ////////
    // Reusing functionality:
    // ////

    // Find redundant instructions in toKeep and insert them into remove.
    // Update VMapRev accordingly.
    void findRedundantInsts(ValueToValueMapTy &VMap,
			    ValueToValueMapTy &VMapRev,
			    set<Instruction *> &toKeep,
			    set<Instruction *> &toRemove);

    // Remove toRemove in function F
    void removeListed(Function &F, set<Instruction *> &toRemove, ValueToValueMapTy &VMap);

    // Returns true if I should be reused in the execute phase
    bool IsReuseInstruction(Instruction *I, bool ReuseAll, bool ReuseBranchCondition);

    ////////
    // Inserting prefetches, reuses, loads
    ////////

    int insertPrefetches(list<LoadInst *> &toPref, set<Instruction *> &toKeep,
			 bool printRes = false, bool onlyPrintOnSuccess = false);
    int insertReuse(list<LoadInst *> &toReuse, set<Instruction *> &toKeep);

    // Removes all instructions marked as a reuse helper. See insertReuse.
    void removeReuseHelper(Function &F);  

    // Selects which instructions toKeep from the phase that starts at the bloc PhaseEntryBB
    void selectInstructionsToReuseWithinAccess(Function *F, set<Instruction*> *toKeep, set<Instruction*> *toUpdateForNextPhases,
                                   LCDResult MinLCDRequirement, bool ReuseAll = true, bool ReuseBranchCondition = false);

    ////////
    // Creating phases
    ////////

    // Swoopify function F with loads in toHoist. Expects all
    // analysis data to be set up.
    bool swoopifyCore(Function &F, list<LoadInst*> toHoist);

    Phase* createAccessExecuteFunction(Function &F,
                                       list<LoadInst *, allocator<LoadInst *>> &toHoist,
                                       vector<BasicBlock *> &PhaseRoots,
                                       AllocaInst *branch_cond,
                                       bool mergeBranches);

    Phase * createAccessPhases(vector<Phase *> &AccessPhases,
                               Phase &ExecutePhase,
                               vector<BasicBlock *> &PhaseRoots);

    bool createAndAppendExecutePhase(Phase *MainPhase, Phase *ExecutePhase, vector<BasicBlock *> &PhaseRoots);

      // Initializes AccessPhases for function F and the set of loads in toHoist
    void initAccessPhases(Function &F, list<LoadInst *> &toHoist, vector<Phase *> &AccessPhases, AllocaInst *branch_cond, bool mergeBranches);

    // Splits up the laods in toHoist into separate sets, each representing a phase
    void identifyPhaseLoads(list<LoadInst *> &toHoist, vector<set<LoadInst *> *> &AccessPhases, AllocaInst *branch_cond, bool mergeBranches);

    // Creates a phase with all loads in Loads that are required for the loop's CFG
    bool CreatePhaseWithCFGLoads(set<Instruction*> &Loads, set<LoadInst*> &NewPhase, AllocaInst *branch_cond, bool mergeBranches);

    // Creates the access phase P: according to its list of loads to prefetch, reuse and load
    bool createAccessPhase(Phase &P, bool isMain);

    // Append one phase to the other; thereby reusing the values in toKeep that are already
    // computed in Access
    void combinePhases(Phase &Access, Phase &ToAppend, set<Instruction *> &toKeep,
		       string type, int phaseCount);

    // Update the VMaps for all values in toKeep and each following
    // phase to map to the values that P maps to.
    bool updateSucceedingAccessMaps(Phase &P, vector<Phase *> &PhasesToUpdate,
				    set<Instruction *> &toKeep);
  };
}

#endif
