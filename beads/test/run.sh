#!/usr/bin/env bash
# Copyright 2026 Insolent Construct. MIT; see beads/build/LICENSE.
set -euo pipefail
out=${1:?usage: beads/test/run.sh OUTPUT_DIR}
mkdir -p "$out"
out=$(cd "$out" && pwd)
for test in backup delay delay_mode gain granular mix processor trigger wavetable firmware; do
  make -j2 -f "beads/test/$test.mk" "BUILD_ROOT=$out/objects/" "TARGET=$out/${test}_test" test
  echo "PASS $test"
done
bash beads/test/audio_reference.sh "$out"
