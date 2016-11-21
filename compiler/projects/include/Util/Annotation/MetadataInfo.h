//===- Util/Annotation/MetadataInfo.h - MetadataInfo Interface -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file MetadataInfo.h
///
/// \brief MetadataInfo Interface
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// This file defines utilities to read and attach metadata to instructions.
//===----------------------------------------------------------------------===//

#ifndef UTIL_ANNOTATION_METADATAINFO_H
#define UTIL_ANNOTATION_METADATAINFO_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <fstream>
#include <iostream>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

using namespace llvm;

namespace util {

  bool InstrhasMetadataKind(Instruction *ii, std::string mdt);
  bool InstrhasMetadata(Instruction *ii, std::string mdt, std::string mdv);
  bool InstrhasMetadataSubstring(Instruction *ii, std::string mdt,
                                 std::string mdv);
  void AttachMetadata(Instruction *inst, std::string mdtype, std::string str);

  void AttachMetadataToAllInBlock(BasicBlock *b, std::string mdtype,
                                  std::string str);

  /* if I is a memory instruction it has an ID attached */
  std::string getInstructionID(Instruction *I);

  /* if I is a memory instruction it has an IDphi attached */
  std::string getInstructionIDphi(Instruction *I);

  std::string getInstructionMD(Instruction *I, const char *MDty);
}

#endif
