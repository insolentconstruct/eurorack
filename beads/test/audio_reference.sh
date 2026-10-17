#!/usr/bin/env bash
# Copyright 2026 Insolent Construct. MIT; see beads/build/LICENSE.
set -euo pipefail
out=${1:?usage: audio_reference.sh OUTPUT_DIR}
mkdir -p "$out"
out=$(cd "$out" && pwd)
for variant in current reference; do
  include=()
  [[ "$variant" != reference ]] || include+=(REFERENCE_INCLUDE=-Ibeads/test/reference)
  make -j2 -f beads/test/audio_reference.mk "BUILD_ROOT=$out/objects/" \
    "TARGET=$out/audio-$variant" "${include[@]}"
  ASAN_OPTIONS=detect_leaks=0 "$out/audio-$variant" "$out/$variant.f32"
done
cmp "$out/current.f32" "$out/reference.f32"
echo 'PASS bit-identical host default audio: 20 scenarios against preserved pre-optimization DSP'
