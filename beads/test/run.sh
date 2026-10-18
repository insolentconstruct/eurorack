#!/usr/bin/env bash
# Copyright 2026 Insolent Construct. MIT; see beads/build/LICENSE.
set -euo pipefail
out=${1:?usage: beads/test/run.sh OUTPUT_DIR}
if [[ ${BEADS_TEST_CONTAINER:-0} != 1 ]]; then
  mkdir -p "$out"
  out=$(cd "$out" && pwd)
  root=$(git rev-parse --show-toplevel)
  stock=$(mktemp -d)
  trap 'rm -rf -- "$stock"' EXIT
  mkdir -p "$stock/beads"
  for file in background_buffer_backup.cc background_buffer_backup.h settings.cc settings.h; do
    git show "f4cfa76:beads/$file" > "$stock/beads/$file"
  done
  docker run --rm --platform linux/amd64 --network none \
    -e BEADS_TEST_CONTAINER=1 -e JOBS="${JOBS:-$(n=$(nproc); echo $(( n < 4 ? n : 4 )))}" -e UPDATE="${UPDATE:-0}" \
    -e STOCK_SOURCE_DIR=/stock-source -v "$stock:/stock-source:ro" \
    -v "$root:/src:$( [[ ${UPDATE:-0} == 1 ]] && echo rw || echo ro )" \
    -v "$out:/out" "${BEADS_BUILD_IMAGE:-beads-build-kit:gcc9}" \
    bash beads/test/run.sh /out
  exit
fi
mkdir -p "$out"
out=$(cd "$out" && pwd)
# Suite files are the registry; audio capture has its own driver below.
for suite in beads/test/*.mk; do
  test=${suite##*/}
  test=${test%.mk}
  case "$test" in suite|audio_reference) continue ;; esac
  make -j"${JOBS:-$(n=$(nproc); echo $(( n < 4 ? n : 4 )))}" -f "$suite" "BUILD_ROOT=$out/objects/" "TARGET=$out/${test}_test" test
  echo "PASS $test"
done
bash beads/test/storage_compat.sh "$out/storage"
bash beads/test/audio_reference.sh "$out" --optimizations
