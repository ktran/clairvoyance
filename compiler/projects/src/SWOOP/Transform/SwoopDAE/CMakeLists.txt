
# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
add_library(SwoopDAE SHARED
  SwoopDAE.cpp
  ${PROJECTS_MAIN_SRC_DIR}/Util/Annotation/MetadataInfo.cpp
  ${PROJECTS_MAIN_SRC_DIR}/Util/Analysis/AliasUtils.cpp
  ${PROJECTS_MAIN_SRC_DIR}/Util/DAE/DAEUtils.cpp
  ${PROJECTS_MAIN_SRC_DIR}/Util/Analysis/LoopDependency.cpp
  ${PROJECTS_MAIN_SRC_DIR}/Util/Transform/BranchMerge/BranchMerge.cpp
  ${SWOOP_MAIN_INCLUDE_DIR}
  ${PROJECTS_MAIN_INCLUDE_DIR}
  LCDHandler.cpp
  FindInstructions.cpp
  ../PhaseStitching.cpp
  ../
  )

target_compile_options(SwoopDAE PRIVATE -fPIC)
