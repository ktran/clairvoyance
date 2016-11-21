
# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
add_library(CFGIndirectionCount MODULE
  CFGIndirectionCount.cpp
  ${PROJECTS_MAIN_SRC_DIR}/Util/Annotation/MetadataInfo.cpp
  ${PROJECTS_MAIN_SRC_DIR}/Util/Analysis/LoopDependency.cpp
  ${PROJECTS_MAIN_SRC_DIR}/Util/Analysis/AliasUtils.cpp
  )
