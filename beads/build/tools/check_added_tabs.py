#!/usr/bin/env python3
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct

"""Reject tabs in C/C++ lines added since BASE (env). Existing code is never checked."""
import os
import subprocess
import sys

diff = subprocess.check_output([
    "git", "diff", "--no-ext-diff", "--no-color", "--unified=0",
    os.environ["BASE"], "HEAD", "--", ":(glob)beads/**/*.h", ":(glob)beads/**/*.cc",
], text=True)
in_hunk = False
bad = False
for line in diff.splitlines():
    if line.startswith("diff --git "):
        in_hunk = False
    elif line.startswith("@@ "):
        in_hunk = True
    elif in_hunk and line.startswith("+") and "\t" in line[1:]:
        print("Tab in added C/C++ line: " + repr(line[1:]), file=sys.stderr)
        bad = True
sys.exit(1 if bad else 0)
