#!/bin/bash
echo "--- Running Code Quality Check ----"

#Checkpatch for Kernel Code
scripts/checkpatch.pl --file ../kernel/nxp_simtemp.c
#Format Python Code
clang-format -i ../user/cli/main.py