
# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.
add_library(LoopExtract MODULE LoopExtract.cpp)

get_property(MODULE_FILE TARGET LoopExtract PROPERTY LOCATION)
#configure_file(run.sh.in run.sh @ONLY)
