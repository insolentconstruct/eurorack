#!/usr/bin/env bash
# Copyright 2026 Insolent Construct. MIT; see beads/build/LICENSE.
set -euo pipefail
out=${1:?usage: audio_reference.sh OUTPUT_DIR [--optimizations]}
[[ $# -le 2 && ( $# == 1 || $2 == --optimizations ) ]] || exit 2
mkdir -p "$out"
out=$(cd "$out" && pwd)
variants=(current)
[[ ${2:-} != --optimizations ]] || variants+=(reference)
for variant in "${variants[@]}"; do
  include=()
  [[ "$variant" != reference ]] || include+=(REFERENCE_INCLUDE=-Ibeads/test/reference)
  make -j"${JOBS:-$(n=$(nproc); echo $(( n < 4 ? n : 4 )))}" -f beads/test/audio_reference.mk "BUILD_ROOT=$out/objects/" \
    "TARGET=$out/audio-$variant" "${include[@]}"
  ASAN_OPTIONS=detect_leaks=0 "$out/audio-$variant" "$out/$variant.f32"
done
reference=$PWD/beads/test/audio_reference.sha256
if [[ ${UPDATE:-0} == 1 ]]; then
  (cd "$out" && sha256sum current.f32) > "$reference"
else
  (cd "$out" && sha256sum -c "$reference")
fi
if [[ ${2:-} == --optimizations ]]; then
  cmp "$out/current.f32" "$out/reference.f32"
fi
echo 'PASS audio reference: 20 scenarios'
