//===----BranchMerge.h - Early evaluation of branches and later merge----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file BranchMerge.h
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// This file provides functions to evaluate branches at an early stage,
// and to decide whether to run the optimized (merged branches) or unoptimized
// (original) version.
//
//===----------------------------------------------------------------------===//

#ifndef COMPILER_BRANCHMERGE_H
#define COMPILER_BRANCHMERGE_H

#include <vector>
#include <string>

#include "Util/Annotation/MetadataInfo.h"

using namespace util;


static std::pair<std::string, std::string> getProbabilityBranch(BranchInst *BI) {
  if (BI->getNumSuccessors() < 2) {
    return std::make_pair("1.0", "0.0");
  }
  std::string string_bp0 = getInstructionMD(&*BI, "BranchProb0");
  std::string string_bp1 = getInstructionMD(&*BI, "BranchProb1");
  if (string_bp0.length() > 0 && string_bp1.length() > 0 ) {
    return std::make_pair(string_bp0, string_bp1);
  }
  return std::make_pair("0.0", "0.0");
};



// pair represents: <isReducable, whichBranch>
static std::pair<bool, int> isReducableBranch(BranchInst *BI, double THRESHOLD) {
  if (BI->getNumSuccessors() < 2) {
    return std::make_pair(false, 0);
  }
  // We don't want to minimize the branches generated through
  // loopChunk ("stdin" tag represents the global virtual iterator)
  if (BI->isConditional()) {
    if (Value *vbi = dyn_cast<Value>(BI->getCondition())) {
      if (vbi->getName().str().find("stdin") != std::string::npos) {
        return std::make_pair(false, 0);
      }
    }
  }

  std::string string_bp0 = getInstructionMD(&*BI, "BranchProb0");
  std::string string_bp1 = getInstructionMD(&*BI, "BranchProb1");
  if (string_bp0.length() > 0 && string_bp1.length() > 0 ) {
    double bp0 = std::stod(string_bp0);
    double bp1 = std::stod(string_bp1);
    if (bp0 > THRESHOLD) {
      return std::make_pair(true, 0);
    } else if (bp1 > THRESHOLD) {
      return std::make_pair(true, 1);
    }
  }
  return std::make_pair(false, 0);
}

static StoreInst* insertFlagCheck(BranchInst *BI, AllocaInst *branch_cond, float BranchProbThreshold) {
  IRBuilder<> Builder(BI);
  float threshold = BranchProbThreshold;
  std::pair<bool, int> res = isReducableBranch(BI, threshold);
  if (res.first) {
    Value *new_cond;
    LoadInst *branch_value = Builder.CreateLoad(branch_cond);
    std::pair<std::string, std::string> bp = getProbabilityBranch(BI);
    if (res.second == 0) {
      errs() << "Assuming probably true: " << *BI << "\n";
      errs() << "With probability: " << bp.first << "\n";
      new_cond = Builder.CreateAnd(branch_value, BI->getCondition());
    } else if (res.second == 1) {
      errs() << "Assuming probably false: " << *BI << "\n";
      errs() << "With probability: " << bp.second << "\n";
      Value *n = Builder.CreateNot(BI->getCondition());
      new_cond = Builder.CreateAnd(branch_value, n);
    } else {
      assert(false && "Couldn't get branch probability.\n");
    }
    return Builder.CreateStore(new_cond, branch_cond);
  }
}

bool minimizeFunctionFromBranchPred(LoopInfo *LI, Function *cF, double threshold);
#endif //COMPILER_BRANCHMERGE_H
