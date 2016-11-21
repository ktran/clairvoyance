
# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
add_library(OptimisticSwoop SHARED
  ../SwoopDAE/SwoopDAE.cpp
  OptimisticSwoop.cpp
  ${PROJECTS_MAIN_SRC_DIR}/Util/Annotation/MetadataInfo.cpp
  ${PROJECTS_MAIN_SRC_DIR}/Util/Analysis/AliasUtils.cpp
  ${PROJECTS_MAIN_SRC_DIR}/Util/DAE/DAEUtils.cpp
  ${PROJECTS_MAIN_SRC_DIR}/Util/Analysis/LoopDependency.cpp
  ${PROJECTS_MAIN_SRC_DIR}/Util/Transform/BranchMerge/BranchMerge.cpp
  ${SWOOP_MAIN_INCLUDE_DIR}
  ${PROJECTS_MAIN_INCLUDE_DIR}
  ../PhaseStitching.cpp
  ../SwoopDAE/LCDHandler.cpp
  ../SwoopDAE/FindInstructions.cpp
  ../
  )

target_compile_options(OptimisticSwoop PRIVATE -fPIC)
add_dependencies(OptimisticSwoop SwoopDAE)
