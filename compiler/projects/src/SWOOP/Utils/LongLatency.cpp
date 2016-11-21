//===- LongLatency.cpp - Kill unsafe stores with backups
//--------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file LongLatency.cpp
///
/// \brief
///
/// \copyright Eta Scale AB. Licensed under the Eta Scale Open Source License. See
/// the LICENSE file for details.
//
// Description of pass ...
//
//===----------------------------------------------------------------------===//
#ifndef LongLatency_
#define LongLatency_

#include "Util/Annotation/MetadataInfo.h"
#include <list>

using namespace util;

// Instructions are marked as Long Latency I with metadata information
bool isLongLatency(Instruction *I) {
  return InstrhasMetadata(I, "Latency", "Long");
}

// Adds pointer to all long latency LoadInsts in F to LoadList.
void findDelinquentLoads(Function &F, list<LoadInst *> &LoadList) {
  for (inst_iterator iI = inst_begin(F), iE = inst_end(F); iI != iE; ++iI) {
    if (LoadInst::classof(&(*iI)) && isLongLatency(&(*iI))) {
      LoadList.push_back((LoadInst *)&(*iI));
    }
  }
}

#endif
