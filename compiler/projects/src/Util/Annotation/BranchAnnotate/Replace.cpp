//===--------------- Replace.cpp - Replaces conditional branch------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file Replace.cpp
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
//===----------------------------------------------------------------------===//
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <type_traits>
#include <fstream>

#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"

#include "llvm/Transforms/Utils/Cloning.h"


#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "Util/Annotation/MetadataInfo.h"

#define F_KERNEL_SUBSTR "__kernel__"
#define EXECUTE_SUFFIX "_execute"
#define CLONE_SUFFIX "_clone"

BasicBlock *deepCopyBB(BasicBlock *src) {
    BasicBlock *dst = BasicBlock::Create(src->getContext());
    for (BasicBlock::iterator iI = src->begin(), iE = src->end(); iI != iE; ++iI) {
        Instruction *c = iI->clone();
        dst->getInstList().push_back(c);
    }
    return dst;
}

std::pair<string, string> getProbabilityBranch(BranchInst *BI) {
    if (BI->getNumSuccessors() < 2) {
        return std::make_pair("1.0", "0.0");
    }
    std::string string_bp0 = getBranchProb(BI, "BranchProb0");
    std::string string_bp1 = getBranchProb(BI, "BranchProb1");
    if (string_bp0.length() > 0 && string_bp1.length() > 0 ) {
        return std::make_pair(string_bp0, string_bp1);
    }
    return std::make_pair("0.0", "0.0");
}

// pair represents: <isReducable, whichBranch>
std::pair<bool, int> isReducableBranch(BranchInst *BI, double THRESHOLD) {
    if (BI->getNumSuccessors() < 2) {
        return std::make_pair(false, 0);
    }
    // We don't want to minimize the branches generated through
    // loopChunk ("stdin" tag represents the global virtual iterator)
    if (BI->isConditional()) {
        if (Value *vbi = dyn_cast<Value>(BI->getCondition())) {
            if (vbi->getName().str().find("stdin") != string::npos) {
                return std::make_pair(false, 0);
            }
        }
    }
    std::string string_bp0 = getBranchProb(BI, "BranchProb0");
    std::string string_bp1 = getBranchProb(BI, "BranchProb1");
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

void replaceBranch(BasicBlock *block, double THRESHOLD) {
    TerminatorInst *TInst = block->getTerminator();
    IRBuilder<> Builder(TInst);
    BranchInst *uncondBI;
    BasicBlock *dst, *comp;
    if (BranchInst *BI = dyn_cast<BranchInst>(TInst)) {
        std::pair<bool, int> res = isReducableBranch(BI, THRESHOLD);
        if (res.first) {
            if (res.second == 0) {
                dst = BI->getSuccessor(0);
                comp = BI->getSuccessor(1);
                uncondBI = BranchInst::Create(dst);
                AttachMetadata(uncondBI, "BranchProb0", "1");
                AttachMetadata(uncondBI, "BranchProb1", "0");
                comp->removePredecessor(block);
                ReplaceInstWithInst(TInst, uncondBI);
            } else if (res.second == 1) {
                dst = BI->getSuccessor(1);
                comp = BI->getSuccessor(0);
                uncondBI = BranchInst::Create(dst);
                AttachMetadata(uncondBI, "BranchProb0", "0");
                AttachMetadata(uncondBI, "BranchProb1", "1");
                comp->removePredecessor(block);
                ReplaceInstWithInst(TInst, uncondBI);
            }
        }
    }
}

bool hasEnding (std::string fullString, std::string ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

/*
bool nameContains(Function &F, std::string s) {
    return
        F.getName().str().find(s) != std::string::npos;
}
*/

bool minimizeFunctionFromBranchPred(Function *cF, double threshold) {
    errs() << "Optimizing function: " << cF->getName().str() << "\n";
    for (Function::iterator block = cF->begin(), blockEnd = cF->end(); block != blockEnd; ++block) {
        replaceBranch(block, threshold);
    }
    return true;
}

bool isFKernel(Function &F) {
    return
        F.getName().str().find(F_KERNEL_SUBSTR) != std::string::npos;
}
