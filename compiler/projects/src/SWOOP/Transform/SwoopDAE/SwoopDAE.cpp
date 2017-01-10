//===--------------- SwoopKernel.cpp - Swoop with DAE ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file SwoopDAE.cpp
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// This file implements a pass to identify every function with "_kernel_"
// as part of the name. Every such clone will be clonedand a call to the
// clone will be added after all calls to the original function. The original
// (cloned) functions will then have every instruction removed except
// those required to follow the control flow graph (CFG), and
// loads of variables visible outside of the enclosing function. Before each
// of these load a prefetch instruction will be added.
//
//===----------------------------------------------------------------------===//

#include "SWOOP/Transform/SwoopDAE/BasicSwoop.h"
#include "../PhaseStitching.h"

#include "../../Utils/DCEutils.cpp"
#include "LCDHandler.h"
#include "FindInstructions.h"

#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "Util/Transform/BranchMerge/BranchMerge.h"

#undef PROFILE

static const char *SWOOPTYPE_TAG = "SwoopType";


// Maximum number of indirections to consider for hoisting
static cl::opt<unsigned> IndirThresh("indir-thresh",
                                     cl::desc("Max number of indirections"),
                                     cl::value_desc("unsigned"));

// Maximum number of indirections to consider for hoisting
static cl::opt<bool> ReuseBranchCondition("reuse-branch-conditions",
                                          cl::desc("Reuse computed branch conditions in addition to loads."),
                                          cl::init(false));


static cl::opt<bool> ReuseAll("reuse-all",
                              cl::desc("Reuse all computation in addition to loads."),
                              cl::init(false));

// Hoists only marked delinquent loads if set
static cl::opt<bool> HoistDelinquent("hoist-delinquent",
                                     cl::desc("Hoisting delinquent loads"),
                                     cl::init(true));

// Generates multi-access code if set
static cl::opt<bool> MultiAccess("multi-access",
                                 cl::desc("Creating multi access phase"),
                                 cl::init(false));

// Specifies the number of times the loop in focus was unrolled
static cl::opt<unsigned> UnrollCount("unroll", cl::desc("Unroll count"),
                                     cl::init(1));

static cl::opt<bool> OptimizeBranches("merge-branches", cl::desc("If set, it will apply branch merge optimizations"),
					  cl::Hidden);

static cl::opt<float> BranchProbThreshold("branch-prob-threshold",
                                          cl::desc("Reduce branch if branch_prob > branch-prob-threshold. Should be larger or equal to 0.5."),
                                          cl::init(0.5));

using namespace llvm;
using namespace std;


namespace swoop {

// Returns true iff F is the main function.
static bool isMain(Function &F) { return F.getName().str().compare("main") == 0; }

// Returns true if Inst is a CFG instruction (Terminator or Phi)
static const bool isCFGInst(Instruction *Inst) {
  if (TerminatorInst::classof(Inst))
    return true;

  if (PHINode::classof(Inst))
    return true;

  return false;
}

static Instruction* insertEmptyAsmUse(Value *V, Instruction *InsertAfter, Type *T, LLVMContext &Context) {
  vector<Type *> ArgTypes;
  ArgTypes.push_back(T);

  std::string SideEffectConstraint = "~{dirflag},~{fpsr},~{flags},~{memory}";
  FunctionType *AsmFTy = FunctionType::get(Type::getVoidTy(Context), ArgTypes, false);
  InlineAsm *IA = InlineAsm::get(AsmFTy, "", "r" + SideEffectConstraint, true, false);
  vector<Value *> AsmArgs(1, V);

  CallInst *PseudoCall = CallInst::Create(IA, AsmArgs, "", InsertAfter);

  // Mark as a pseudo instruction to be removed at a later stage
  AttachMetadata(PseudoCall, SWOOPTYPE_TAG, "ReuseHelper");

  return PseudoCall;
}

void SwoopDAE::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<AAResultsWrapperPass>();
  AU.addRequired<LoopInfoWrapperPass>();
  AU.addRequired<TargetTransformInfoWrapperPass>();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addRequired<PostDominatorTree>();
  AU.addRequired<AssumptionCacheTracker>();
  AU.addRequired<TargetLibraryInfoWrapperPass>();
}

bool SwoopDAE::runOnModule(Module &M) {
  bool change = false;

  for (Module::iterator fI = M.begin(), fE = M.end(); fI != fE; ++fI) {
    // Check if function should be swoopified
    if (isSwoopKernel(*fI)) {
      errs() << "\n";
      errs() << fI->getName() << ":\n";
      change |= swoopify(*fI);
    }
    else if (isMain(*fI)) {
      //        insertCallInitPAPI(&*fI);
      change = true;
    }
  }

  return change;
}

bool SwoopDAE::isSwoopKernel(Function &F) {
  return F.getName().str().find(F_KERNEL_SUBSTR) != string::npos &&
      F.getName().str().find(CLONE_SUFFIX) == string::npos;
}


void SwoopDAE::divideLoads(list<LoadInst *> &toHoist,
                           list<LoadInst *> &toPref,
                           list<LoadInst *> &toReuse,
                           list<LoadInst *> &toLoad,
                           unsigned int UnrollCount) {
  for (auto P = toHoist.begin(), PE = toHoist.end(); P != PE; ++P) {
    set<Instruction *> Deps;
    getRequirementsInIteration(AA, LI, *P, Deps);

    LCDResult DepLCD = getLCDUnion(AA, LI, Deps);
    LCDResult Res = getLCDInfo(AA, LI, *P, UnrollCount);

    if (DepLCD == LCDResult::NoLCD && Res == LCDResult::NoLCD) {
      toReuse.push_back(*P);
    } else {
      toPref.push_back(*P);
    }
    // toLoad remains empty: conservative version
  }
}

int SwoopDAE::insertReuse(list<LoadInst *> &toReuse, set<Instruction *> &toKeep) {
  int reuseCount = 0;
  for (auto L = toReuse.begin(), LE = toReuse.end(); L != LE; ++L) {
    set<Instruction *> Deps;

    bool hoistable = followDeps(AA, *L, Deps);
    if (!hoistable) {
      // not hoistable: load depends on instruction
      // that requires a global store / call
      continue;
    }

    // increase number of successfully hoisted loads
    // for reuse
    ++reuseCount;

    // Insert pseudo instruction using the load, otherwise it will be removed
    // when simplifying the function. This pseudo instruction will be only
    // inserted temporarily
    LLVMContext &Context = (*L)->getParent()->getParent()->getContext();
    Type *Type = (*L)->getType();
    Instruction *PseudoUse = insertEmptyAsmUse(*L, (*L)->getParent()->getTerminator(), Type, Context);

    // keep all dependencies of this load, and the load itself
    toKeep.insert(Deps.begin(), Deps.end());
    toKeep.insert(*L);
    toKeep.insert(PseudoUse);
  }

  return reuseCount;
}

  bool SwoopDAE::CreatePhaseWithCFGLoads(set<Instruction*> &Loads, set<LoadInst*> &NewPhase, AllocaInst *branch_cond, bool mergeBranches) {
  vector<Loop *> Loops (LI->begin(), LI->end());
  assert(Loops.size() == 1 && "Swoop only works on single loops!");
  Loop *SwoopLoop = Loops.front();

  set<Instruction*> LoopCFGTerminators;
  BasicBlock *Latch = SwoopLoop->getLoopLatch();
  if (Latch->getTerminator()->getNumSuccessors() == 1) {
    Latch = Latch->getSinglePredecessor();
  };

  set<Instruction *> toKeep;
  bool ReducableBranchExists = false;

  if (OptimizeBranches && mergeBranches) {
    for (Instruction *LoadI : Loads) {
      // Check whether it is a reducable branch
      getRequirementsInIteration(AA, LI, LoadI, LoopCFGTerminators);
    }

    SmallVector < BasicBlock * , 4 > SwoopExitingBlocks;
    SwoopLoop->getExitingBlocks(SwoopExitingBlocks);

    for (BasicBlock *ExitingBlock : SwoopExitingBlocks) {
      TerminatorInst *TI = ExitingBlock->getTerminator();
      LoopCFGTerminators.insert(TI);
    }

    for (Instruction *I : LoopCFGTerminators) {
      if (BranchInst * BI = dyn_cast<BranchInst>(I)) {
        if (isReducableBranch(BI, BranchProbThreshold).first && Latch != BI->getParent()) {
          toKeep.insert(BI);
          getRequirementsInIteration(AA, LI, BI, toKeep);
          StoreInst *Store = insertFlagCheck(BI, branch_cond, BranchProbThreshold);
          AttachMetadata(Store, SWOOPTYPE_TAG, "DecisionBlock");
          ReducableBranchExists = true;
        }
      }
    }
  }

  // Insert all requirements of the loop latch (they will
  //be inserted into the first access phase any way)
  getRequirementsInIteration(AA, LI, Latch->getTerminator(), toKeep);

  set<Instruction *> CFGLoads;
  set_intersection(toKeep.begin(), toKeep.end(), Loads.begin(),
                   Loads.end(), inserter(CFGLoads, CFGLoads.begin()));

  if (!CFGLoads.empty()) {
    for (Instruction *CFGL : CFGLoads) {
      NewPhase.insert(static_cast<LoadInst*>(CFGL));
    }
  }
  return ReducableBranchExists || !CFGLoads.empty();
}

void SwoopDAE::identifyPhaseLoads(list<LoadInst *> &toHoist,
                                  vector<set<LoadInst *> *> &AccessPhases,
                                  AllocaInst *branch_cond,
				  bool mergeBranches) {
  set<Instruction *> Remaining(toHoist.begin(), toHoist.end());

  set<LoadInst *> *AccessPhase = new set<LoadInst *>();
  bool KeepInitialPhase = CreatePhaseWithCFGLoads(Remaining, *AccessPhase, branch_cond, mergeBranches);
  if (KeepInitialPhase) {
    for (auto L = AccessPhase->begin(), LE = AccessPhase->end(); L != LE; ++L) {
      Remaining.erase(*L);
    }

    AccessPhases.push_back(AccessPhase);
  } else {
    delete(AccessPhase);
  }

  if (Remaining.empty()) {
    // All elements are required for the CFG, in this case
    // there is no meaning to do multiaccess
    return;
  }

  if (!MultiAccess) {
    AccessPhase = new set<LoadInst *>();
    for (auto L = Remaining.begin(), LE = Remaining.end(); L != LE; ++L) {
      AccessPhase->insert((LoadInst *)*L);
    }
    AccessPhases.push_back(AccessPhase);
    return;
  }

  // For each load, find the set of other loads in toHoist that it
  // depends on
  map<Instruction *, set<Instruction *> *> LoadDeps;
  for (auto L = Remaining.begin(), LE = Remaining.end(); L != LE; ++L) {
    set<Instruction *> Deps;
    getRequirementsInIteration(AA, LI, *L, Deps);
    set<Instruction *> *RelevantDeps = new set<Instruction *>();

    // find the relevant requirements / deps: the ones that
    // are loads themselves
    set_intersection(Deps.begin(), Deps.end(), Remaining.begin(),
                     Remaining.end(),
                     inserter(*RelevantDeps, RelevantDeps->begin()));
    LoadDeps[*L] = RelevantDeps;
  }

  // Split into access phases: an access phase should be created
  // such that there are no dependencies between loads within the phase.
  while (!Remaining.empty()) {
    AccessPhase = new set<LoadInst *>();
    // Contains all loads that do not have dependencies in the remaining
    // set of loads
    for (auto L = Remaining.begin(), LE = Remaining.end(); L != LE; ++L) {
      set<Instruction *> &AllDeps = *(LoadDeps[*L]);
      set<Instruction *> RemainingDeps;
      set_intersection(Remaining.begin(), Remaining.end(), AllDeps.begin(),
                       AllDeps.end(),
                       inserter(RemainingDeps, RemainingDeps.begin()));
      if (RemainingDeps.empty()) {
        // No dependencies remain that are not already hoisted.
        // Safely insert this load to this access phase.
        AccessPhase->insert((LoadInst *)*L);
      }
    }

    // Remove all loads from the current access phase
    for (auto L = AccessPhase->begin(), LE = AccessPhase->end(); L != LE; ++L) {
      Remaining.erase(*L);
    }

    AccessPhases.push_back(AccessPhase);
  }

  // Free resources
  for (auto D = LoadDeps.begin(), DE = LoadDeps.end(); D != DE; ++D) {
    delete (D->second);
  }
}

bool SwoopDAE::createAccessPhase(Phase &P, bool isMain) {
  set<Instruction *> toKeep;
  Function &Access = *(P.F);

  // Find Instructions required to follow the CFG.
  findTerminators(Access, toKeep);

  // Find function entry instructions
  BasicBlock &EntryBlock = Access.getEntryBlock();
  for (auto I = EntryBlock.begin(), IE = EntryBlock.end(); I != IE; ++I) {
    toKeep.insert(&*I);
  }

  if (isMain && OptimizeBranches) {
    for (auto I = inst_begin(P.F), IE = inst_end(P.F); I != IE; ++I) {
      if (InstrhasMetadataKind(&*I, SWOOPTYPE_TAG)) {
        if ("DecisionBlock" == getInstructionMD(&*I, SWOOPTYPE_TAG)) {
          toKeep.insert(&*I);
        }
      }
    }
  }

  // Follow CFG dependencies
  set<Instruction *> Deps;

  // no need to check res: we check the dependencies
  // at an earlier stage
  bool res = followDeps(AA, toKeep, Deps);

  // Keep all data dependencies
  toKeep.insert(Deps.begin(), Deps.end());

  if (isMain) {
    LLVMContext &Context = Access.getParent()->getContext();

    // Find loop phi nodes
    BasicBlock *LoopHeader = Access.getEntryBlock().getTerminator()->getSuccessor(0);
    for (auto I = LoopHeader->begin(); isa<PHINode>(&*I); ++I) {
      toKeep.insert(&*I);
      Type *Type = (&*I)->getType();
      Instruction *PseudoUse = insertEmptyAsmUse(&*I, LoopHeader->getTerminator(), Type, Context);
      toKeep.insert(PseudoUse);
    }
  }

  int prefs = insertPrefetches(P.ToPref, toKeep, true);
  int reuse = insertReuse(P.ToReuse, toKeep);
  int loads = insertReuse(P.ToLoad, toKeep);

  errs() << "Reuse: " << reuse << ", Prefetches:" << prefs
         << ", Loads:" << loads << ".\n";
  if (prefs == 0 && reuse == 0 && !isMain) {
    errs() << "No suitable loads to swoopify.\n";
    return false; // clone was created
  }

  // remove unwanted instructions from access
  removeUnlisted(Access, toKeep);

  // before simplifying, split block: why? we don't want to remove
  // the phi nodes of the loop header, as we need them for mapping
  // at a later stage. Only simplify the rest
  assert(Access.getEntryBlock().getTerminator()->getNumSuccessors() == 1 && "We assume that the loop header should be the only successor!");
  BasicBlock *LoopHeader = Access.getEntryBlock().getTerminator()->getSuccessor(0);
  SplitBlock(LoopHeader, LoopHeader->getFirstNonPHI());

  // simplify the control flow graph
  // (remove all unnecessary instructions and branches)
  TargetTransformInfo &TTI =
      getAnalysis<TargetTransformInfoWrapperPass>().getTTI(Access);
  simplifyCFG(&Access, TTI);
  return true;
}

void SwoopDAE::initAccessPhases(Function &F,
                                list<LoadInst *> &toHoist,
                                vector<Phase *> &AccessPhases,
                                AllocaInst *branch_cond,
                                bool mergeBranches) {
  list<LoadInst *> toReuse; // LoadInsts to reuse
  list<LoadInst *> toLoad;  // LoadInsts to load in A and re-load in E
  list<LoadInst *> toPref;  // LoadInsts to prefetch
  divideLoads(toHoist, toPref, toReuse, toLoad, UnrollCount);

  // Identify the loads for each access phase
  vector<set<LoadInst *> *> AccessPhaseLoads;
  identifyPhaseLoads(toHoist, AccessPhaseLoads, branch_cond, mergeBranches);

  // For each set of access phase loads..
  for (int i = 0; i < AccessPhaseLoads.size(); ++i) {
    Phase *P = new Phase();
    AccessPhases.push_back(P);

    set<LoadInst *> *ToHoistOriginal = AccessPhaseLoads[i];
    vector<pair<LoadInst *, LoadInst *>> OriginalToCurrentMap;

    if (i == 0) {
      // The first access phase is created from the original function itself
      P->F = &F;
      for (LoadInst *L : *ToHoistOriginal) {
        OriginalToCurrentMap.push_back(make_pair(L, L));
      }
    } else {
      // All other access phases are created by cloning
      P->F = cloneFunction(AccessPhases[i - 1]->F, P->VMap);

      if (mergeBranches) {
        minimizeFunctionFromBranchPred(&getAnalysis<LoopInfoWrapperPass>(*(P->F)).getLoopInfo(),
                                       P->F,
                                       BranchProbThreshold);
      }

      // Map the loads (from the original function) that we need to hoist into
      // this phase
      // to the equivalent loads in this clone
      vector<Phase *> PreviousPhases(AccessPhases.begin() + 1,
                                     AccessPhases.end());
      for (LoadInst *L : *ToHoistOriginal) {
        LoadInst *EquivalentLoad;
        LoadInst *OrigLoad = L;
        for (Phase *Phase : PreviousPhases) {
          EquivalentLoad = dyn_cast<LoadInst>(Phase->VMap[OrigLoad]);
          OrigLoad = EquivalentLoad;
        }
        OriginalToCurrentMap.push_back(make_pair(L, EquivalentLoad));
      }
    }

    // Based on the mapping, decide which loads to reuse/prefetch/load for
    // later.
    for (auto LoadMapping : OriginalToCurrentMap) {
      if (find(toLoad.begin(), toLoad.end(), LoadMapping.first) !=
          toLoad.end()) {
        P->ToLoad.push_back(LoadMapping.second);
      } else if (find(toPref.begin(), toPref.end(), LoadMapping.first) !=
          toPref.end()) {
        P->ToPref.push_back(LoadMapping.second);
      } else if (find(toReuse.begin(), toReuse.end(), LoadMapping.first) !=
          toReuse.end()) {
        P->ToReuse.push_back(LoadMapping.second);
      }
    }
  }

  for (auto L : AccessPhaseLoads) {
    delete (L);
  }
}

bool SwoopDAE::updateSucceedingAccessMaps(Phase &P,
                                          vector<Phase *> &PhasesToUpdate,
                                          set<Instruction *> &toKeep) {
  vector<Phase *>::iterator ToUpdateEnd = PhasesToUpdate.end();

  // For each instruction to keep
  for (Instruction *I : toKeep) {
    vector<Phase *>::iterator ToUpdateIter = PhasesToUpdate.begin();

    Value *CurrentV = P.VMap[I];
    while (ToUpdateIter != ToUpdateEnd) {
      Phase *PhaseToUpdate = *ToUpdateIter;
      if (PhaseToUpdate->VMap.find(I) == PhaseToUpdate->VMap.end()) {
        Value *Tmp = PhaseToUpdate->VMap[CurrentV];
        PhaseToUpdate->VMap[I] = Tmp;
        CurrentV = Tmp;
      }
      ++ToUpdateIter;
    }
  }
}

bool SwoopDAE::isWorthTransforming(Function &F, list<LoadInst*> &Loads) {
  std::vector<Loop *> Loops(LI->begin(), LI->end());
  assert(Loops.size() == 1 && "After modification we should only have one loop!");

  Loop *LoopToTransform = Loops.at(0);

  SmallVector < BasicBlock * , 4 > ExitingBlocks;
  LoopToTransform->getExitingBlocks(ExitingBlocks);

  set<Instruction*> Deps;
  for (BasicBlock *B : ExitingBlocks) {
    TerminatorInst *TI = B->getTerminator();
    getRequirementsInIteration(AA, LI, TI, Deps);
    Deps.insert(TI);
  }

  int branchCount = 0;
  for (Instruction *Inst : Deps) {
    if (isa<TerminatorInst>(Inst)) {
      ++branchCount;
    }
  }

  errs() << "Heuristic: " << Loads.size() << " Loads, " << branchCount << " Branches.\n";
  if (Loads.size() / (double) branchCount < 0.5) {
    return false;
  }

  return true;
}

bool SwoopDAE::swoopify(Function &F) {
  LI = &getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
  DT = &getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
  PDT = &getAnalysis<PostDominatorTree>(F);

  // We need to manually construct BasicAA directly in order to disable
  // its use of other function analyses.
  BasicAAResult BAR(createLegacyPMBasicAAResult(*this, F));

  // Construct our own AA results for this function. We do this manually to
  // work around the limitations of the legacy pass manager.
  AAResults AAR(createLegacyPMAAResults(*this, F, BAR));
  AA = &AAR;

  list<LoadInst *> Loads, toHoist;   // LoadInsts to hoist
  findAccessInsts(AA, LI, F, Loads, HoistDelinquent, IndirThresh);

  // filter loads on LCDS (data & control dependencies)
  filterLoadsOnLCD(AA, LI, Loads, toHoist, UnrollCount);
  unsigned int BadLCDDeps = Loads.size() - toHoist.size();

  errs() << "Indir: " << IndirThresh << ", " << toHoist.size() << " load(s) in access phase.\n";
  errs() << "(BadLCDDeps: " << BadLCDDeps << ")\n";

  if (!isWorthTransforming(F, toHoist)) {
    errs() << "Transformation not suitable for this loop.\n";
    return false;
  }

  if (toHoist.empty()) {
    errs() << "Disqualified: no loads to hoist\n";
    return false;
  }

  bool succeeded = swoopifyCore(F, toHoist);
  return succeeded;
}

AllocaInst *initBranchCheckVar(Function *access) {
  IRBuilder<> Builder(&*(access->getEntryBlock().getTerminator()->getSuccessor(0)->getFirstInsertionPt()));
  AllocaInst *bc = Builder.CreateAlloca(Type::getInt1Ty(getGlobalContext()), 0, "branch_flag");
  StoreInst *S = Builder.CreateStore(llvm::ConstantInt::get(Type::getInt1Ty(getGlobalContext()),1), bc);
  AttachMetadata(S, SWOOPTYPE_TAG, "DecisionBlock");

  return bc;
}

bool SwoopDAE::swoopifyCore(Function &F, list<LoadInst*> toHoist) {
  AllocaInst *branch_cond = initBranchCheckVar(&F);

  // Access phases initialization, optimized AE with merging branches
  vector<BasicBlock *> PhaseRoots;

  // Initialize alternative: has to be done before the optimized phase is created,
  // otherwise it clones the wrong function
  ValueToValueMapTy VMap, VMapRev;
  Function *FAlternative;
  list<LoadInst *> toHoistMapped;

  if (OptimizeBranches) {
    FAlternative = cloneFunction(&F, VMap);
    for (LoadInst *L : toHoist) {
      toHoistMapped.push_back(dyn_cast<LoadInst>(VMap[L]));
    }
  }

  bool mergeBranches = true;
  Phase *MainPhase = createAccessExecuteFunction(F, toHoist, PhaseRoots, branch_cond, mergeBranches);
  if (!MainPhase) {
    return false;
  }

  if (OptimizeBranches) {
    // Access phases initialization, unoptimized AE
    LI = &getAnalysis<LoopInfoWrapperPass>(*FAlternative).getLoopInfo();

    mergeBranches = false;
    Phase *AlternativePhase =
        createAccessExecuteFunction(*FAlternative, toHoistMapped, PhaseRoots, branch_cond, mergeBranches);
    if (!AlternativePhase) {
      return false;
    }

    LI = &getAnalysis<LoopInfoWrapperPass>(*(MainPhase->F)).getLoopInfo();

    set<Instruction *> toReuseInExecute, toKeep, toRemove;
    selectInstructionsToReuseWithinAccess(MainPhase->F, &toKeep, &toReuseInExecute, LCDResult::MayLCD);
    findRedundantInsts(VMap, VMapRev, toReuseInExecute, toRemove);

    // remove unwanted instructions from execute
    removeListed(*AlternativePhase->F, toRemove, VMapRev);

    // replace function arguments with first access phase
    replaceArgs(AlternativePhase->F, MainPhase->F);

    BasicBlock *DecisionBlock = *(pred_begin(PhaseRoots.at(1)));
    stitchAEDecision(*(MainPhase->F),
                     *(AlternativePhase->F),
                     VMapRev,
                     branch_cond,
                     *(pred_begin(PhaseRoots.at(1))),
                     *LI,
                     *DT,
                     "original",
                     1000);

    delete(FAlternative); // == AlternativePhase->F
    delete(AlternativePhase);
  }


  LI = &getAnalysis<LoopInfoWrapperPass>(*(MainPhase->F)).getLoopInfo();
  DT->recalculate(*(MainPhase->F));
  PDT = &getAnalysis<PostDominatorTree>(*(MainPhase->F));


  // insert phi nodes wherever a value is not defined for all predecessors
  ensureStrictSSA(*(MainPhase->F), *LI, *DT, PhaseRoots);

  delete(MainPhase);

  return true;
}

Phase* SwoopDAE::createAccessExecuteFunction(Function &F, list<LoadInst *, allocator<LoadInst *>> &toHoist,
                                           vector<BasicBlock *> &PhaseRoots, AllocaInst *branch_cond,
                                           bool mergeBranches) {
  
  // Access phase initialization
  vector<Phase*> AccessPhases;
  initAccessPhases(F, toHoist, AccessPhases, branch_cond, mergeBranches);

  // Execute phase initialization
  Phase ExecutePhase;
  ExecutePhase.F = cloneFunction(AccessPhases[AccessPhases.size() - 1]->F, ExecutePhase.VMap);

  LI = &getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
  DT->recalculate(F);
  PDT = &getAnalysis<PostDominatorTree>(F);

  Phase *AEFunction = createAccessPhases(AccessPhases, ExecutePhase, PhaseRoots);

  if (AEFunction != nullptr) {
    createAndAppendExecutePhase(AEFunction, &ExecutePhase, PhaseRoots);
  }

  // Clean up
  for (Phase *P : AccessPhases) {
    if (!P || AEFunction != P) {
      delete(P->F);
      delete(P);
    }
  }
  delete ExecutePhase.F;

  return AEFunction;
}

bool SwoopDAE::createAndAppendExecutePhase(Phase *MainPhase, Phase *ExecutePhase, vector<BasicBlock *> &PhaseRoots) {
  set<Instruction *> toReuseInExecute, toKeep;

  selectInstructionsToReuseInExecute(MainPhase->F, &toKeep /* not used */, &toReuseInExecute, acceptedForReuse(), ReuseAll, ReuseBranchCondition);
  PhaseRoots.push_back(&(ExecutePhase->F->getEntryBlock()));

  combinePhases(*MainPhase,*ExecutePhase, toReuseInExecute, "execute", 100000 /* CHANGE! DO WE NEED THIS? */);
  removeReuseHelper(*(MainPhase->F));

  // Update loop info and dominator tree
  LI = &getAnalysis<LoopInfoWrapperPass>(*(MainPhase->F)).getLoopInfo();
}


Phase * SwoopDAE::createAccessPhases(vector<Phase *> &AccessPhases,
                                     Phase &ExecutePhase,
                                     vector<BasicBlock *> &PhaseRoots) {
  // Keep track of the first access phase
  Phase *FirstAccessPhase = AccessPhases.at(0);
  //BasicBlock *DecisionBlock = LI->getLoopFor(FirstAccessPhase->F->getEntryBlock().getTerminator()->getSuccessor(0))->getUniqueExitBlock();

  // Instructions to keep throughout all access phases
  set<Instruction *> toKeep;

  for (int i = 0; i < AccessPhases.size(); ++i) {
    errs() << "Processing Access Phase " << i << "\n";
    Phase *P = AccessPhases.at(i);

    bool success = createAccessPhase(*P, P == FirstAccessPhase);
    if (!success) {
      if (FirstAccessPhase == P && (i + 1 < AccessPhases.size())) {
        FirstAccessPhase = AccessPhases.at(i+1);
        P->F->replaceAllUsesWith(FirstAccessPhase->F);
      } else {
        return nullptr;
      }
      P->F->eraseFromParent();
    }

    BasicBlock *EntryBlock = &(P->F->getEntryBlock());
    PhaseRoots.push_back(EntryBlock);

    combinePhases(*FirstAccessPhase, *P, toKeep, "access", i);

    set<Instruction *> toUpdateForNextPhases; // Instructions to keep from this phase
    selectInstructionsToReuseWithinAccess(FirstAccessPhase->F, &toKeep, &toUpdateForNextPhases, LCDResult::MayLCD);

    // Update succeeding phases to use values in toUpdateForNextPhases instead of
    // current vmap values
    if (i + 1 < AccessPhases.size()) {
      vector<Phase *> NextPhases(AccessPhases.begin() + i + 2, AccessPhases.end());
      NextPhases.push_back(&ExecutePhase);
      updateSucceedingAccessMaps(*(AccessPhases.at(i + 1)), NextPhases, toUpdateForNextPhases);
    }

    // Update loop info and dominator tree
    LI = &getAnalysis<LoopInfoWrapperPass>(*(FirstAccessPhase->F)).getLoopInfo();
    DT->recalculate(*(FirstAccessPhase->F));
    PDT = &getAnalysis<PostDominatorTree>(*(FirstAccessPhase->F));
  }

  return FirstAccessPhase;
}

LCDResult SwoopDAE::acceptedForReuse() {
  return LCDResult::NoLCD;
}

bool SwoopDAE::IsReuseInstruction(Instruction *I, bool ReuseAll, bool ReuseBranchCondition) {
  if (ReuseAll) {
    return true;
  }

  if (LoadInst::classof(&*I)) {
    return true;
  }

  if (ReuseBranchCondition) {
    bool IsBranchCond = false;
    for (Value::user_iterator U = (&*I)->user_begin(), UE = (&*I)->user_end();
         U != UE && !IsBranchCond; ++U) {
      if (TerminatorInst::classof(*U)) {
        IsBranchCond = true;
      }
    }

    if (IsBranchCond) {
      return true;
    }
  }
  return false;
}

void SwoopDAE::selectInstructionsToReuseInExecute(Function *F, set<Instruction*> *toKeep, set<Instruction*> *toUpdateForNextPhases, LCDResult MinLCDRequirement, bool ReuseAll, bool ReuseBranchCondition) {
  selectInstructionsToReuseWithinAccess(F, toKeep, toUpdateForNextPhases, MinLCDRequirement, ReuseAll, ReuseBranchCondition);
}

void SwoopDAE::selectInstructionsToReuseWithinAccess(Function *F, set<Instruction*> *toKeep, set<Instruction*> *toUpdateForNextPhases, LCDResult MinLCDRequirement, bool ReuseAll, bool ReuseBranchCondition) {
  set<Instruction *> ReuseCandidates;

  // First find all loads and their requirements: they are reuse candidates
  list<LoadInst*> LoadList;
  findRelevantLoads(*F, LoadList, HoistDelinquent);
  for (LoadInst* Load : LoadList) {
    if (toKeep->find(Load) != toKeep->end()) {
      // already in the set of to keep, skip
      continue;
    }

    getRequirementsInIteration(AA, LI, Load, ReuseCandidates);
    ReuseCandidates.insert(Load);
  }

  BasicBlock &EntryBlock = F->getEntryBlock();
  for (BasicBlock::iterator I = EntryBlock.begin(), IE = EntryBlock.end(); I != IE; ++I) {
    if (toKeep->insert(&*I).second) {
      toUpdateForNextPhases->insert(&*I);
    }
  }

  for (BasicBlock::iterator I = EntryBlock.getUniqueSuccessor()->begin(); isa<PHINode>(&*I); ++I) {
    if (toKeep->insert(&*I).second) {
      toUpdateForNextPhases->insert(&*I);
    }
  }


  // For each candidate: find the requirements and check
  // if it meets the reuse condition
  for (Instruction *Candidate : ReuseCandidates) {
    if (toKeep->find(Candidate) != toKeep->end()) {
      // already processed
      continue;
    }

    set<Instruction*> Deps;
    getRequirementsInIteration(AA, LI, Candidate, Deps);
    Deps.insert(Candidate);

    if (expectAtLeast(AA, LI, Deps, MinLCDRequirement)) {
      for (Instruction *Dependency : Deps) {
        if (IsReuseInstruction(Dependency, ReuseAll, ReuseBranchCondition)) {
          if (toKeep->insert(Dependency).second) {
            toUpdateForNextPhases->insert(Dependency);
          }
        }
      }
    }
  }
}

void SwoopDAE::removeReuseHelper(Function &F) {
  list<Instruction *> toErase;
  for (auto I = inst_begin(&F), IE = inst_end(&F); I != IE; ++I) {
    if (InstrhasMetadataKind(&*I, SWOOPTYPE_TAG)) {
      if ("ReuseHelper" == getInstructionMD(&*I, SWOOPTYPE_TAG)) {
        toErase.push_back(&*I);
      }
    }
  }

  for (auto I : toErase) {
    I->eraseFromParent();
  }
}

void SwoopDAE::combinePhases(Phase &Access, Phase &ToAppend,
                             set<Instruction *> &toKeep, string type,
                             int phaseCount) {
  if (&Access == &ToAppend) {
    FunctionType *AsmFTy =
        FunctionType::get(Type::getVoidTy(Access.F->getContext()), false);
    InlineAsm *IA = InlineAsm::get(
        AsmFTy, "0:", "~{dirflag},~{fpsr},~{flags},~{memory}", true, false);
    vector<Value *> AsmArgs;
    CallInst::Create(IA, Twine(""), Access.F->getEntryBlock().getTerminator());
    return;
  }

  set<Instruction *> toRemove; // Instructions to remove

  // map instructions from A to their counterpart in E
  findRedundantInsts(ToAppend.VMap, ToAppend.VMapRev, toKeep,
                     toRemove);

  // remove unwanted instructions from execute
  removeListed(*(ToAppend.F), toRemove, ToAppend.VMapRev);

  errs() << "Reusing " << toRemove.size() << " instructions. \n";

  // replace function arguments with first access phase
  replaceArgs(ToAppend.F, Access.F);

  // stitch function to access phase
  stitch(*(Access.F), *(ToAppend.F), ToAppend.VMap, ToAppend.VMapRev, *LI, *DT, false, type, phaseCount);
}

// Inserts a prefetch for every LoadInst in toPref
// that fulfils the criterion of being inserted.
// All prefetches to be kept are added to toKeep
// (more unqualified prefetches may be added to the function).
// Returns the number of inserted prefetches.
int SwoopDAE::insertPrefetches(list<LoadInst *> &toPref,
                               set<Instruction *> &toKeep, bool printRes,bool onlyPrintOnSuccess) {
  map<LoadInst *, pair<CastInst *, CallInst *>> prefs;
  set<Instruction *> prefToKeep;

  int total = 0, ins = 0;

  // Insert prefetches
  for (list<LoadInst *>::iterator I = toPref.begin(), E = toPref.end(); I != E; I++) {
    // Set the indir thresh very high - we don't want to consider
    // indirections from the previous iteration and filtered indirections already
    // in filterLoadsOnIndir.
    unsigned MaxIndirThresh = 100;
    PrefInsertResult res = insertPrefetch(AA, *I, prefToKeep, prefs, MaxIndirThresh);
    if (res == Inserted) {
      ++ins;
    }
  }

  // Print results
  toKeep.insert(prefToKeep.begin(), prefToKeep.end());

  return ins;
}

void SwoopDAE::findRedundantInsts(ValueToValueMapTy &VMap,
                                  ValueToValueMapTy &VMapRev,
                                  set<Instruction *> &toKeep,
                                  set<Instruction *> &toRemove) {

  set<Instruction *>::iterator kI = toKeep.begin(), kE = toKeep.end();
  ValueToValueMapTy::iterator vI = VMap.begin(), vE = VMap.end();
  for (; kI != kE; kI++) {
    vI = VMap.begin();
    while (vI != vE) {
      if (vI->first && vI->second) {
        if ((*kI) == vI->first) {
          if (isa<CmpInst>(*kI)) {
            Instruction *AccessCmp = dyn_cast<Instruction>(*kI);
            Instruction *ExecuteCmp = dyn_cast<Instruction>(vI->second);

	    assert ((ExecuteCmp || OptimizeBranches)
		    && "Branch must be existent in both maps, if branches are not optimized");

	    if (!ExecuteCmp) {
	      vI++;
	      continue;
	    }

            // If the comparison type is not the same as before - recompute
            // the value to be on the safe side
            if (!AccessCmp->isSameOperationAs(ExecuteCmp)) {
              vI++;
              continue;
            }
          }
          toRemove.insert(dyn_cast<Instruction>(vI->second));
          VMapRev.insert(std::pair<Value *, Value *>(
              dyn_cast<Instruction>(vI->second), *kI));
        }
      }
      vI++;
    }
  }
}

/*Avoid duplication of address and conditions computation in execute*/
void SwoopDAE::removeListed(Function &F, set<Instruction *> &toRemove,
                            ValueToValueMapTy &VMap) {
  set<Instruction *>::iterator ksI = toRemove.begin(), ksE = toRemove.end();
  inst_iterator iI = inst_begin(F), iE = inst_end(F);
  while (iI != iE) {
    Instruction *Inst = &(*iI);
    iI++;
    if (find(ksI, ksE, Inst) != ksE && (!isCFGInst(Inst))) {
      assert(VMap[Inst]);
      Inst->replaceAllUsesWith(VMap[Inst]);
      Inst->eraseFromParent();
    }
  }
}

void SwoopDAE::filterLoadsOnLCD(AliasAnalysis *AA,
                                LoopInfo *LI,
                                list<LoadInst *> &Loads,
                                list<LoadInst *> &FilteredLoads,
                                unsigned int UnrollCount) {
  list<LoadInst *>::iterator L = Loads.begin();
  while (L != Loads.end()) {
    // Only hoist loads that are not necessarily known to be an LCD
    set<Instruction *> Deps;
    getRequirementsInIteration(AA, LI, *L, Deps);

    bool depsNoLCD = expectAtLeast(AA, LI, Deps, LCDResult::NoLCD);
    if (depsNoLCD && getLCDInfo(AA, LI, *L, UnrollCount) < LCDResult::MustLCD) {
      FilteredLoads.push_back(*L);
    }
    ++L;
  }
}

char SwoopDAE::ID = 0;
static RegisterPass<SwoopDAE> B("dae-swoop", "SwoopDAE_pass", false, false);

}
