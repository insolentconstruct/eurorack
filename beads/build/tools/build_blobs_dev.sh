#!/usr/bin/env bash
# shellcheck disable=SC2016  # make expands $(...), not the shell.
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
#
# A development build with Plaits blobs, in the build image from the Drizzlet
# root: the blobs, their default region, the catalogue (which must equal
# beads/blob/plaits_catalog.cc), the app with PLAITS_BLOBS=1, and the image
# of app and region as a flash write would leave them.
#   beads/build/tools/build_blobs_dev.sh OUT
set -euo pipefail
out=${1:?usage: build_blobs_dev.sh OUT}
mkdir -p "$out"
unset MAKEFLAGS MFLAGS MAKELEVEL
release=$(sed -n 's/^const uint32_t kPlaitsBlobRelease = \([0-9]*\);$/\1/p' \
  beads/blob/plaits_catalog.cc)
bash beads/build/tools/build_blobs.sh "$out/blob"
blobs=("$out/blob/plaits_wavetables.bin" "$out/blob/speech_library.bin" \
  "$out/blob/wavetable.bin" "$out/blob/speech.bin" \
  "$out/blob/particle_noise.bin" "$out/blob/spectral_spiral.bin" \
  "$out/blob/phase_weave.bin")
python3 beads/build/tools/blob_sections.py "$out/region.bin" "$release" \
  "${blobs[@]}"
python3 beads/build/tools/blob_catalog.py "$out/plaits_catalog.cc" \
  "$release" "$out/region.bin.json" "${blobs[@]}"
if ! cmp -s beads/blob/plaits_catalog.cc "$out/plaits_catalog.cc"; then
  diff beads/blob/plaits_catalog.cc "$out/plaits_catalog.cc" || true
  echo "beads/blob/plaits_catalog.cc is stale: copy $out/plaits_catalog.cc"
  exit 1
fi
mkdir -p "$out/objects/beads"
make --no-print-directory -o beads/resources.cc -o beads/resources.h \
  -f beads/build/link-order.mk KIT_MAKEFILE=beads/makefile \
  "BUILD_ROOT=$out/objects/" "TOOLCHAIN_PATH=$TOOLCHAIN_PATH" KIT_GCC9=1 \
  PLAITS_BLOBS=1 -j"$(nproc)" bin > "$out/app.log" 2>&1 \
  || { tail -40 "$out/app.log" >&2; exit 1; }
elf=$out/objects/beads/beads.elf
cp "$out/objects/beads/beads.bin" "$out/app.bin"
cp "$elf" "$out/app.elf"
begin=$("${TOOLCHAIN_PATH}bin/arm-none-eabi-nm" "$elf" \
  | awk '$3 == "plaits_region_begin" { print $1 }')
# The app, erased flash up to the region, then the region.
python3 - "$out/app.bin" "$out/region.bin" "$out/image.bin" "$begin" <<'EOF'
import sys
app = open(sys.argv[1], 'rb').read()
region = open(sys.argv[2], 'rb').read()
begin = int(sys.argv[4], 16) - 0x08020000
# Hygrostat places the region at the next 4 KiB boundary after app.bin.
if begin != (len(app) + 4095) // 4096 * 4096:
  raise SystemExit('the region does not start after app.bin')
open(sys.argv[3], 'wb').write(app + b'\xff' * (begin - len(app)) + region)
print('app %d B, region at 0x%08x, %d B' % (len(app), begin + 0x08020000,
                                           len(region)))
EOF
sha256sum "$out/app.bin" "$out/region.bin" "$out/image.bin"
