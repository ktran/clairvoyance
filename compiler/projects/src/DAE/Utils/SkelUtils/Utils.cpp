//===- Utils.cpp - Determines which functions to target for DAE------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file Utils.cpp
///
/// \brief Determines which functions to target for DAE
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//===----------------------------------------------------------------------===//
using namespace llvm;
using namespace std;

#ifndef Utils_
#define Utils_
#include "llvm/Analysis/LoopAccessAnalysis.h"
#include "../../../SWOOP/Utils/LongLatency.cpp"
#include "DAE/Utils/SkelUtils/headers.h"
#include <algorithm>
#include <llvm/IR/BasicBlock.h>

void declareExternalGlobal(Value *v, int val);
bool toBeDAE(Function *F);
bool isDAEkernel(Function *F);
bool isMain(Function *F);

bool isDAEkernel(Function *F) {
  bool ok = false;
  size_t found = F->getName().str().find("_clone");
  size_t found1 = F->getName().str().find("__kernel__");
  if ((found == std::string::npos) && (found1 != std::string::npos))
    ok = true;
  return ok;
}

void declareExternalGlobal(Value *v, int val) {
  std::string path = "Globals.ll";
  std::error_code err;
  llvm::raw_fd_ostream out(path.c_str(), err, llvm::sys::fs::F_Append);

  out << "\n@\"" << v->getName() << "\" = global i64 " << val << "  \n";
  out.close();
}

/////////////////////////////////////////////////////////////
//
//              From LoopVectorize.cpp
//
/////////////////////////////////////////////////////////////


/// Utility class for getting and setting loop vectorizer hints in the form
/// of loop metadata.
/// This class keeps a number of loop annotations locally (as member variables)
/// and can, upon request, write them back as metadata on the loop. It will
/// initially scan the loop for existing metadata, and will update the local
/// values based on information in the loop.
/// We cannot write all values to metadata, as the mere presence of some info,
/// for example 'force', means a decision has been made. So, we need to be
/// careful NOT to add them if the user hasn't specifically asked so.
class LoopVectorizeHints {
  enum HintKind {
    HK_WIDTH,
    HK_UNROLL,
    HK_FORCE
  };

  /// Hint - associates name and validation with the hint value.
  struct Hint {
    const char * Name;
    unsigned Value; // This may have to change for non-numeric values.
    HintKind Kind;

    Hint(const char * Name, unsigned Value, HintKind Kind)
      : Name(Name), Value(Value), Kind(Kind) { }

    bool validate(unsigned Val) {
      switch (Kind) {
      case HK_WIDTH:
        return true; //isPowerOf2_32(Val) && Val <= VectorizerParams::MaxVectorWidth;
      case HK_UNROLL:
        return true; //isPowerOf2_32(Val) && Val <= MaxInterleaveFactor;
      case HK_FORCE:
        return (Val <= 1);
      }
      return false;
    }

  };

  /// Vectorization width.
  Hint Width;
  /// Vectorization interleave factor.
  Hint Interleave;
  /// Vectorization forced
  Hint Force;

  /// Return the loop metadata prefix.
  static StringRef Prefix() { return "llvm.loop."; }

public:
  enum ForceKind {
    FK_Undefined = -1, ///< Not selected.
    FK_Disabled = 0,   ///< Forcing disabled.
    FK_Enabled = 1,    ///< Forcing enabled.
  };

  LoopVectorizeHints(const Loop *L, bool DisableInterleaving)
      : Width("vectorize.width", VectorizerParams::VectorizationFactor,
              HK_WIDTH),
        Interleave("interleave.count", DisableInterleaving, HK_UNROLL),
        Force("vectorize.enable", FK_Undefined, HK_FORCE),
        TheLoop(L) {
    // Populate values with existing loop metadata.
    getHintsFromMetadata();

    // force-vector-interleave overrides DisableInterleaving.
    if (VectorizerParams::isInterleaveForced())
      Interleave.Value = VectorizerParams::VectorizationInterleave;

    //DEBUG(if (DisableInterleaving && Interleave.Value == 1) dbgs()
    //      << "LV: Interleaving disabled by the pass manager\n");
  }

  unsigned getWidth() const { return Width.Value; }
  unsigned getInterleave() const { return Interleave.Value; }
  enum ForceKind getForce() const { return (ForceKind)Force.Value; }
private:
  /// Find hints specified in the loop metadata and update local values.
  void getHintsFromMetadata() {
    MDNode *LoopID = TheLoop->getLoopID();
    if (!LoopID)
      return;

    // First operand should refer to the loop id itself.
    assert(LoopID->getNumOperands() > 0 && "requires at least one operand");
    assert(LoopID->getOperand(0) == LoopID && "invalid loop id");

    for (unsigned i = 1, ie = LoopID->getNumOperands(); i < ie; ++i) {
      const MDString *S = nullptr;
      SmallVector<Metadata *, 4> Args;

      // The expected hint is either a MDString or a MDNode with the first
      // operand a MDString.
      if (const MDNode *MD = dyn_cast<MDNode>(LoopID->getOperand(i))) {
        if (!MD || MD->getNumOperands() == 0)
          continue;
        S = dyn_cast<MDString>(MD->getOperand(0));
        for (unsigned i = 1, ie = MD->getNumOperands(); i < ie; ++i)
          Args.push_back(MD->getOperand(i));
      } else {
        S = dyn_cast<MDString>(LoopID->getOperand(i));
        assert(Args.size() == 0 && "too many arguments for MDString");
      }

      if (!S)
        continue;

      // Check if the hint starts with the loop metadata prefix.
      StringRef Name = S->getString();
      if (Args.size() == 1)
        setHint(Name, Args[0]);
    }
  }

  // /// Checks string hint with one operand and set value if valid.
  void setHint(StringRef Name, Metadata *Arg) {
    if (!Name.startswith(Prefix()))
      return;
    Name = Name.substr(Prefix().size(), StringRef::npos);

    const ConstantInt *C = mdconst::dyn_extract<ConstantInt>(Arg);
    if (!C) return;
    unsigned Val = C->getZExtValue();

    Hint *Hints[] = {&Width, &Interleave, &Force};
    for (auto H : Hints) {
      if (Name == H->Name) {
        if (H->validate(Val))
          H->Value = Val;
        else
          // DEBUG(dbgs() << "LV: ignoring invalid hint '" << Name << "'\n");
        break;
      }
    }
  }

  /// The loop these hints belong to.
  const Loop *TheLoop;
};

int loopToBeDAE(Loop *L, std::string benchmarkName,
                bool requireDelinquent = true) {

  // Only accept inner-most loops
  if (L->getSubLoops().size() != 0) {
    return false;
  }
      
  int MAGIC_TRANSFORM = 1337;

  // If any of the parent loops has a hint,
  // it is a loop to be transformed
  Loop *Parent = L;
  while (Parent) {
    LoopVectorizeHints Hints(Parent, false);
    if (Hints.getWidth() >= MAGIC_TRANSFORM) {
      return true;
    }
    Parent = Parent->getParentLoop();
  }
  
  return false;
}

bool isMain(Function *F) { return F->getName().str().compare("main") == 0; }

bool toBeDAE(Function *F) {
  bool ok = false;

  /*401.bzip*/
  if (F->getName().str().compare("BZ2_compressBlock") == 0) // 13% generateMTFValues
      ok = true;
  if (F->getName().str().compare("BZ2_decompress") == 0) // 12%
    ok = true;
  // if (F->getName().str().compare("mainGtU") == 0) // 16% no loads found
  //   ok = true;

  /*429.mcf*/
  if (F->getName().str().compare("primal_bea_mpp") == 0) // 38%
    ok = true;

  /*433.milc*/
  if (F->getName().str().compare("mult_su3_na") == 0) // 15%
    ok = true;

  /*450.soplex*/
  if (F->getName().str().compare("_ZN6soplex8SSVector19assign2productShortERKNS_5SVSetERKS0_") == 0) // 19%
    ok = true;
  if (F->getName().str().compare(
  				 "_ZN6soplex10SPxSteepPR9entered4XENS_5SPxIdEiiiii") == 0) // 14%
    ok = true;
  if (F->getName().str().compare("_ZN6soplex8SSVector5setupEv") == 0) // 11%
    ok = true;

  /*456.hmmer*/
  if (F->getName().str().compare("P7Viterbi") == 0)  // 87%
    ok = true;

  /*458.sjeng*/
  if (F->getName().str().compare("std_eval") == 0) // 17%
    ok = true;

  /*462.libQ*/
  if (F->getName().str().compare("quantum_toffoli") == 0) // 60%
    ok = true;
  if (F->getName().str().compare("quantum_sigma_x") == 0) // 23%
    ok = true;
  if (F->getName().str().compare("quantum_cnot") == 0) // 12%
    ok = true;


  /*470.lbm*/
  if (F->getName().str().compare("LBM_performStreamCollide") == 0) // 99%
    ok = true;

  /*464.h264ref*/
  if (F->getName().str().compare("SetupFastFullPelSearch") == 0) // 18%
    ok = true;
  if (F->getName().str().compare("BlockMotionSearch") == 0) // 13%
    ok = true;

  /*473.astar*/
  if (F->getName().str().compare("_ZN6wayobj10makebound2EPiiS0_") == 0) // 22%
    ok = true;
  if (F->getName().str().compare("_ZN7way2obj12releaseboundEv") == 0) // 34%
    ok = true;

  /*482.sphinx3*/
  if (F->getName().str().compare("mgau_eval") == 0) // 39%
    ok = true;

  /*403.gcc*/
  if (F->getName().str().compare("reg_is_remote_constant_p") == 0) // 16%
    ok = true;

  /*400.perlbench*/
  if (F->getName().str().compare("S_regmatch") == 0) // 35%
    ok = true;

  /*445.gobmk*/
  if (F->getName().str().compare("fastlib") == 0) // 5% - no loop 
    ok = true;
  if (F->getName().str().compare("do_play_move") == 0) //5% - no loop
    ok = true;
  if (F->getName().str().compare("do_dfa_matchpat") == 0) //4%
    ok = true;
  if (F->getName().str().compare("dfa_matchpat_loop") == 0) //4%
    ok = true;
  if (F->getName().str().compare("incremental_order_moves") == 0) //4%
    ok = true;



  /*471.omnetpp*/
  if (F->getName().str().compare("getFirst") == 0) // 13% no loop
    ok = true;
  if (F->getName().str().compare("_ZN12cMessageHeap7shiftupEi") == 0) // part of getFirst
    ok = true;


  /*473.xalancbmk*/
  if (F->getName().str().compare("_ZN11xercesc_2_510ValueStore13isDuplicateOfEPNS_17DatatypeValidatorEPKtS2_S4_") == 0) // 6%
    ok = true;
  if (F->getName().str().compare("_ZN11xercesc_2_510ValueStore8containsEPKNS_13FieldValueMapE") == 0) // 17% - not swoopifiable loops
    ok = true;

  /*444.namd*/
  if (F->getName().str().compare("_ZN20ComputeNonbondedUtil26calc_pair_energy_fullelectEP9nonbonded") == 0) // 11%
    ok = true;
  if (F->getName().str().compare("_ZN20ComputeNonbondedUtil16calc_pair_energyEP9nonbonded") == 0) // 11%
    ok = true;
  if (F->getName().str().compare("_ZN20ComputeNonbondedUtil32calc_pair_energy_merge_fullelectEP9nonbonded") == 0) // 11%
    ok = true;
  if (F->getName().str().compare("_ZN20ComputeNonbondedUtil19calc_pair_fullelectEP9nonbonded") == 0) // 11%
    ok = true;

  /*447.dealII*/
  if (F->getName().str().compare("_ZNK13LaplaceSolver6SolverILi3EE15assemble_matrixERNS1_12LinearSystemERK18TriaActiveIteratorILi3E15DoFCellAccessorILi3EEES9_RN7Threads16DummyThreadMutexE") == 0) // 11%
    ok = true;

  /*453.povray*/
  if (F->getName().str().compare("_ZN3povL31All_CSG_Intersect_IntersectionsEPNS_13Object_StructEPNS_10Ray_StructEPNS_13istack_structE") == 0) // 14%
    ok = true;

  /*331.art_l*/
  if (F->getName().str().compare("compute_train_match") == 0) // 14%
    ok = true;
  if (F->getName().str().compare("compute_values_match") == 0) // 14%
    ok = true;

  /*CG*/
  if (F->getName().str().compare("conj_grad") == 0) // 6%
    ok = true;

  /*LU*/
  if (F->getName().str().compare("blts") == 0) // 14%
    ok = true;
  if (F->getName().str().compare("buts") == 0) // 14%
    ok = true;

  /*UA*/
  if (F->getName().str().compare("diffusion") == 0) // 12%
    ok = true;
  if (F->getName().str().compare("transfb") == 0) // 6%
    ok = true;


  //return ok;
  return true; // for artifact evaluation, making it easier
}



#endif
