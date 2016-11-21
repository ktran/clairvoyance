
# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
set(PROJECTS_MAIN_SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(PROJECTS_MAIN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

include_directories(${PROJECTS_MAIN_INCLUDE_DIR})

add_subdirectory(src)
