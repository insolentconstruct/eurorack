#!/usr/bin/env bash
# Copyright 2026 Insolent Construct. MIT; see beads/build/LICENSE.
set -euo pipefail
# Suite files are the registry; storage and audio have separate drivers.
suites=()
for suite in beads/test/*.mk; do
  test=${suite##*/}
  test=${test%.mk}
  case "$test" in suite|audio_reference) continue ;; esac
  suites+=("$test")
done
if [[ $# == 1 && $1 == --list ]]; then
  printf '%s\n' "${suites[@]}"
  exit
fi
usage() {
  echo 'usage: beads/test/run.sh --list | OUTPUT_DIR' >&2
  exit 2
}
[[ $# == 1 && $1 != -* ]] || usage
out=$1
if [[ ${BEADS_TEST_CONTAINER:-0} != 1 ]]; then
  mkdir -p "$out"
  out=$(cd "$out" && pwd)
  root=$(git rev-parse --show-toplevel)
  stock=$(mktemp -d)
  container=
  trap '[[ -z $container ]] || docker rm -f "$container" >/dev/null; rm -rf -- "$stock"' EXIT
  mkdir -p "$stock/beads"
  for file in background_buffer_backup.cc background_buffer_backup.h settings.cc settings.h; do
    git show "f4cfa76:beads/$file" > "$stock/beads/$file"
  done
  if [[ -n ${DOCKER_CONTEXT:-} ]]; then
    # Remote contexts cannot bind-mount local paths: copy sources in, results out.
    container=$(docker create --platform linux/amd64 --network none -w /src \
      -e BEADS_TEST_CONTAINER=1 -e JOBS="${JOBS:-4}" -e UPDATE="${UPDATE:-0}" \
      -e STOCK_SOURCE_DIR=/stock-source "${BEADS_BUILD_IMAGE:-beads-build-kit:gcc9}" \
      bash beads/test/run.sh /out)
    tar -C "$root" --exclude=.git --exclude=build --exclude='*_test' \
      -cf - beads stmlib stm_audio_bootloader tools | docker cp - "$container:/src"
    docker cp "$root/beads/build" "$container:/src/beads/build"
    docker cp "$stock" "$container:/stock-source"
    status=0
    docker start -a "$container" || status=$?
    docker cp "$container:/out/." "$out"
    if [[ ${UPDATE:-0} == 1 && $status == 0 ]]; then
      docker cp "$container:/src/beads/test/audio_reference.sha256" "$root/beads/test/audio_reference.sha256"
    fi
    exit "$status"
  fi
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
for test in "${suites[@]}"; do
  suite=beads/test/$test.mk
  make -j"${JOBS:-$(n=$(nproc); echo $(( n < 4 ? n : 4 )))}" -f "$suite" "BUILD_ROOT=$out/objects/" "TARGET=$out/${test}_test" test
  echo "PASS $test"
done
bash beads/test/storage_compat.sh "$out/storage"
bash beads/test/audio_reference.sh "$out" --optimizations
