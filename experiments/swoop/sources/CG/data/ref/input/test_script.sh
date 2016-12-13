#!/bin/bash

REFERENCE=$1
OUTPUT=$2

grep -i "Verification successful" $OUTPUT
rc=$?
exit $rc
