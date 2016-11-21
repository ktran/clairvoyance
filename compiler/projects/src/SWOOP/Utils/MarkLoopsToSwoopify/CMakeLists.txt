
# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
add_library(MarkLoopsToSwoopify 
  SHARED
  MarkLoopsToSwoopify.cpp
  ${PROJECTS_MAIN_SRC_DIR}/Util/Annotation/MetadataInfo.cpp
  ${PROJECTS_MAIN_INCLUDE_DIR}/Util/Annotation )

