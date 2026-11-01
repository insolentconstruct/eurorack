#!/usr/bin/env bash
# Copyright 2026 Insolent Construct. MIT; see beads/build/LICENSE.
# Preserve build failures and their output while compressing transport only.
set -uo pipefail
out=${1:?output directory required}
archive=${2:?transport archive required}
shift 2
"$@"
status=$?
archive_status=0
tar -C "$out" -czf "$archive" . || archive_status=$?
if [[ "$status" != 0 ]]; then
  exit "$status"
fi
exit "$archive_status"
