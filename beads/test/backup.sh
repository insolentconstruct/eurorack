#!/usr/bin/env bash
# Run inside the build image, from the repository root.
set -euo pipefail
out=${1:?usage: beads/test/backup.sh OUTPUT_DIR}
mkdir -p "$out"
g++ -std=c++14 -O2 -ffp-contract=off -DSTM32H7XX -DTEST -Wall -Werror \
  -Wno-int-to-pointer-cast -Wno-unused-local-typedefs \
  -Ibeads/test/backup_stubs -I. \
  beads/test/backup_test.cc beads/background_buffer_backup.cc \
  beads/settings.cc -o "$out/backup"
"$out/backup" "$out"
