#!/usr/bin/env bash
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct (insolentconstruct@gmail.com)
# MIT; see beads/build/LICENSE.
# Run from the repository root, using the working tree and an installed toolchain.
set -euo pipefail
export LC_ALL=C
unset MAKEFLAGS MFLAGS MAKELEVEL
out=${1:?usage: build_dwt.sh ABSOLUTE_OUTPUT_DIR}
[[ $# == 1 && "$out" = /* && "$out" != *[[:space:]]* ]] || exit 2
[[ ! -e "$out" || -z "$(ls -A "$out")" ]] || {
  echo "Output must be empty" >&2
  exit 2
}
: "${TOOLCHAIN_PATH:?Set the GCC 9 toolchain prefix (parent of bin)}"
[[ -f stmlib/makefile.inc ]] || {
  echo "Initialize the pinned stmlib submodule first" >&2
  exit 1
}
mkdir -p "$out"
out=$(cd "$out" && pwd)
"$TOOLCHAIN_PATH/bin/arm-none-eabi-gcc" --version > "$out/compiler-version.txt"
git rev-parse HEAD > "$out/source-head.txt"
for mode in release calibration; do
  flag=0
  [[ "$mode" != calibration ]] || flag=1
  root="$out/$mode/"
  mkdir -p "$root"
  make --no-print-directory -o beads/resources.cc -o beads/resources.h \
    -f beads/build/link-order.mk KIT_MAKEFILE=beads/makefile \
    "BUILD_ROOT=$root" "TOOLCHAIN_PATH=$TOOLCHAIN_PATH" KIT_GCC9=1 \
    "DWT_CALIBRATION=$flag" -j"${JOBS:-8}" bin kit-info \
    > "$out/$mode.log" 2>&1 || {
      tail -60 "$out/$mode.log" >&2
      exit 1
    }
  "$TOOLCHAIN_PATH/bin/arm-none-eabi-size" "$root/beads/beads.elf" \
    > "$out/$mode-size.txt"
  if [[ "$mode" == release ]]; then
    pin=$(awk '$2 == "app.bin" { print $1 }' beads/build/expected-hashes.sha256)
    printf '%s  %s\n' "$pin" release/beads/beads.bin > "$out/release-pin.sha256"
    (cd "$out" && sha256sum -c release-pin.sha256)
  fi
done
# Bind a copy of the ABI-v1 decoder to this calibration application.
python3 - "$out" <<'PYCODE'
import hashlib
from pathlib import Path
import re
import sys

out = Path(sys.argv[1])
sha = hashlib.sha256((out / 'calibration/beads/beads.bin').read_bytes()).hexdigest()
source = Path('beads/build/tools/dwt.py').read_text()
source, count = re.subn(r'^BUILD_SHA = .*$', 'BUILD_SHA = "' + sha + '"',
    source, flags=re.MULTILINE)
assert count == 1
(out / 'dwt.py').write_text(source)
PYCODE
(cd "$out" && sha256sum release/beads/beads.{elf,bin} \
  calibration/beads/beads.{elf,bin} dwt.py > SHA256SUMS)
