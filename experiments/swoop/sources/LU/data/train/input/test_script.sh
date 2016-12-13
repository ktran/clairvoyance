#!/bin/bash
# Copyright (C) Eta Scale AB. Licensed under the Eta Scale Open Source License. See the LICENSE file for details.

REFERENCE=$1
OUTPUT=$2

grep -i "Verification successful" $OUTPUT
rc=$?
exit $rc
