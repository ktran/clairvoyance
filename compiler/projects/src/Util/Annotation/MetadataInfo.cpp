//===- MetadataInfo.cpp - Kill unsafe stores with backups
//--------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file MetadataInfo.cpp
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// Description of pass ...
//
//===----------------------------------------------------------------------===//
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Pass.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"

#include "Util/Annotation/MetadataInfo.h"
#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;

namespace util {

bool InstrhasMetadataKind(Instruction *ii, std::string mdt) {
  unsigned mk = ii->getContext().getMDKindID(mdt);
  if (mk) {
    MDNode *mdn = ii->getMetadata(mk);
    if (mdn)
      return true;
  }

  return false;
}

bool InstrhasMetadata(Instruction *ii, std::string mdt, std::string mdv) {
  unsigned mk = ii->getContext().getMDKindID(mdt);
  if (mk) {
    MDNode *mdn = ii->getMetadata(mk);
    if (mdn) {
      Metadata *mds = mdn->getOperand(0);
      StringRef str;
      if (MDString::classof(mds))
        str = (cast<MDString>(*mds)).getString();
      if (str == mdv)
        return true;
    }
  }

  return false;
}

bool InstrhasMetadataSubstring(Instruction *ii, std::string mdt,
                               std::string mdv) {
  unsigned mk = ii->getContext().getMDKindID(mdt);
  if (mk) {
    MDNode *mdn = ii->getMetadata(mk);
    if (mdn) {
      Metadata *mds = mdn->getOperand(0);
      StringRef str;
      if (MDString::classof(mds)) {
        str = (cast<MDString>(*mds)).getString();
        size_t found = mdv.find(str.str());
        if (found != std::string::npos)
          return true;
      }
    }
  }

  return false;
}

void AttachMetadata(Instruction *inst, std::string mdtype, std::string str) {
  // attach pragma as metadata
  unsigned mk = inst->getContext().getMDKindID(mdtype);
  Metadata *V = MDString::get(inst->getContext(), StringRef(str));
  MDNode *n = MDNode::get(inst->getContext(), V);
  inst->setMetadata(mk, n);
}

void AttachMetadataToAllInBlock(BasicBlock *b, std::string mdtype,
                                std::string str) {
  for (BasicBlock::iterator it = b->begin(); it != b->end(); it++)
    AttachMetadata(&*it, mdtype, str);
}

/* if I is a memory instruction it has an ID attached */
std::string getInstructionID(Instruction *I) {
  std::string str = "empty";
  MDNode *mdn = I->getMetadata("ID");
  if (mdn) {
    Metadata *mds = mdn->getOperand(0);
    StringRef str;
    if (MDString::classof(mds))
      str = (cast<MDString>(*mds)).getString();
    return str;
  }
  return str;
}

/* if I is a memory instruction it has an IDphi attached */
std::string getInstructionIDphi(Instruction *I) {
  std::string str = "empty";
  MDNode *mdn = I->getMetadata("IDphi");
  if (mdn) {
    Metadata *mds = mdn->getOperand(0);
    StringRef str;
    if (MDString::classof(mds))
      str = (cast<MDString>(*mds)).getString();
    return str;
  }
  return str;
}

std::string getInstructionMD(Instruction *I, const char *MDty) {
  std::string str = "";
  MDNode *mdn = I->getMetadata(MDty);
  if (mdn) {
    Metadata *mds = mdn->getOperand(0);
    StringRef str;
    if (MDString::classof(mds))
      str = (cast<MDString>(*mds)).getString();
    return str;
  }
  return str;
}
}
