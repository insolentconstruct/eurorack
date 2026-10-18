#!/usr/bin/env bash
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct

# Released-source golden app.
set -euo pipefail
export LC_ALL=C
unset MAKEFLAGS MFLAGS MAKELEVEL
out=${1:?usage: beads/build/golden.sh EMPTY_OUTPUT_DIR}
mode=${2:-}
[[ $# -le 2 && ( -z "$mode" || "$mode" == --inside ) ]] || exit 2
[[ "$out" = /* && "$out" != *[[:space:],]* ]] || { echo "Use an absolute path without whitespace or commas" >&2; exit 2; }
release=d8f473915232af74347762b6365202bfd26f73f6
stmlib=e3bd7c9cc00e4364166f9905c0509b6ffd0535ec
if [[ "$mode" != --inside ]]; then
  [[ ! -e "$out" || -z "$(ls -A "$out")" ]] || { echo "Output must be empty" >&2; exit 2; }
  root=$(git rev-parse --show-toplevel)
  [[ "$root" != *,* ]] || exit 2
  git cat-file -e "$release^{commit}"
  git -C stmlib cat-file -e "$stmlib^{commit}"
  mkdir -p "$out/source/stmlib"
  git archive "$release" beads | tar -x -C "$out/source"
  git -C stmlib archive "$stmlib" | tar -x -C "$out/source/stmlib"
  mkdir -p "$out/source/beads/build"
  cp beads/build/link-order.mk beads/build/golden.sh "$out/source/beads/build/"
  printf '%s\n' "eurorack $release" "stmlib $stmlib" > "$out/source-commits.txt"
  docker run --rm --platform linux/amd64 --network none --user "$(id -u):$(id -g)" \
    --mount "type=bind,src=$out/source,dst=/src,readonly" --mount "type=bind,src=$out,dst=/out" \
    "${BEADS_BUILD_IMAGE:-beads-build-kit}" nice -n 10 bash beads/build/golden.sh /out --inside
  exit 0
fi
[[ -n "${GOLDEN_TOOLCHAIN_PATH:-}" ]] || { echo "Run in the compiler container" >&2; exit 2; }
mkdir -p "$out/objects/beads"
nice -n 10 make --no-print-directory -o beads/resources.cc -o beads/resources.h \
  -f beads/build/link-order.mk KIT_MAKEFILE=beads/makefile KIT_GCC9=0 \
  "BUILD_ROOT=$out/objects/" "TOOLCHAIN_PATH=$GOLDEN_TOOLCHAIN_PATH" -j2 bin kit-info \
  > "$out/build.log" 2>&1 || { tail -40 "$out/build.log" >&2; exit 1; }
cp "$out/objects/beads/beads.bin" "$out/app.bin"
cp "$out/objects/beads/beads.elf" "$out/app.elf"
# Fixed golden payload.
echo "1f487b48e41dc4d813c5c457de115a4d798d001253ff90b5c5ef1b1a156eee6a  app.bin" | (cd "$out" && sha256sum -c -)
(cd "$out" && sha256sum app.bin app.elf > SHA256SUMS)
