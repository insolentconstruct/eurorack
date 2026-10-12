#!/usr/bin/env bash
#
# Copyright 2026 insolentconstruct.
#
# Author: insolentconstruct
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
# See http://creativecommons.org/licenses/MIT/ for more information.

# Run from repository root: --wav on host; binary-only inside compiler container.
set -euo pipefail
export LC_ALL=C
unset MAKEFLAGS MFLAGS MAKELEVEL
out=${1:?usage: beads/build/build.sh EMPTY_OUTPUT_DIR [--wav]}
mode=${2:-}
[[ $# -le 2 && ( -z "$mode" || "$mode" == --wav ) ]] || exit 2
[[ "$out" = /* && "$out" != *[[:space:]]* ]] || { echo "Output must be an absolute path without whitespace" >&2; exit 2; }
[[ ! -e "$out" || -z "$(ls -A "$out")" ]] || { echo "Output must be empty" >&2; exit 2; }
if [[ "$mode" == --wav ]]; then
  # Host orchestration: encoder stays inside Buster, never copied into Bookworm.
  command -v docker >/dev/null
  root="$(git rev-parse --show-toplevel)"
  [[ "$root" != *,* && "$out" != *,* ]] || { echo "Docker mount paths must not contain commas" >&2; exit 2; }
  [[ "$(git -C "$root/stm_audio_bootloader" rev-parse HEAD)" == d391c6f71eaeaa40ffd5d3d761a6843f675bc2b1 ]] || { echo "Wrong encoder commit" >&2; exit 1; }
  mkdir -p "$out"
  name=${BEADS_RELEASE_NAME:-beads-dev}
  [[ "$name" =~ ^beads-[A-Za-z0-9._-]+$ ]] || { echo "BEADS_RELEASE_NAME must match beads-[A-Za-z0-9._-]+" >&2; exit 2; }
  build_image=${BEADS_BUILD_IMAGE:-beads-build-kit}
  encoder_image=${BEADS_ENCODER_IMAGE:-beads-encoder}
  docker run --rm --platform linux/amd64 --network none --user "$(id -u):$(id -g)" \
    --mount "type=bind,src=$root,dst=/src,readonly" --mount "type=bind,src=$out,dst=/out" "$build_image" \
    nice -n 10 bash beads/build/build.sh /out
  encoder_start=$SECONDS
  docker run --rm --platform linux/amd64 --network none --user "$(id -u):$(id -g)" -w /src \
    --mount "type=bind,src=$root,dst=/src,readonly" --mount "type=bind,src=$out,dst=/out" -e PYTHONPATH=/src "$encoder_image" \
    nice -n 10 /usr/local/bin/qemu-x86_64-static \
    -cpu SandyBridge,-avx,-x2apic,-tsc-deadline /usr/local/bin/python2.7 \
    stm_audio_bootloader/qpsk/encoder.py \
    -t stm32h7 -k -s 48000 -b 12000 -c 6000 -p 256 /out/app.bin
  echo "Pinned-CPU encoding (including container startup): $((SECONDS - encoder_start)) seconds"
  mv "$out/app.wav" "$out/$name.wav"
  # The pinned hash is of the WAV content; the file name follows the release name.
  echo "$(cut -c1-64 "$root/beads/build/expected-wav.sha256")  $name.wav" | (cd "$out" && sha256sum -c -)
  docker run --rm --platform linux/amd64 --network none --user "$(id -u):$(id -g)" \
    --mount "type=bind,src=$root,dst=/src,readonly" --mount "type=bind,src=$out,dst=/out" -e BEADS_RELEASE_NAME="$name" "$build_image" \
    nice -n 10 python3 beads/build/tools/release.py /out
  exit 0
fi
[[ -f stmlib/makefile.inc && -f stm_audio_bootloader/qpsk/encoder.py ]] || { echo "Initialize submodules first" >&2; exit 1; }
mkdir -p "$out/objects/beads" "$out/objects/beads_bootloader"
for spec in 'beads/makefile beads' 'beads/bootloader/makefile beads_bootloader'; do
  read -r mf target <<< "$spec"
  nice -n 10 make --no-print-directory -o beads/resources.cc -o beads/resources.h -f beads/build/link-order.mk \
    "KIT_MAKEFILE=$mf" "BUILD_ROOT=$out/objects/" \
    "TOOLCHAIN_PATH=$TOOLCHAIN_PATH" -j2 bin hex kit-info notice-deps \
    > "$out/$target.log" 2>&1 || { tail -40 "$out/$target.log" >&2; exit 1; }
done
cp "$out/objects/beads/beads.bin" "$out/app.bin"
cp "$out/objects/beads/beads.elf" "$out/app.elf"
cp "$out/objects/beads_bootloader/beads_bootloader.bin" "$out/bootloader.bin"
# Independently reconstruct combo using the upstream HEX merge recipe.
nice -n 10 make --no-print-directory -o beads/resources.cc -o beads/resources.h -f beads/build/link-order.mk \
  KIT_MAKEFILE=beads/makefile "BUILD_ROOT=$out/objects/" \
  "TOOLCHAIN_PATH=$TOOLCHAIN_PATH" "$out/objects/beads/beads_bootloader_combo.bin" \
  > "$out/combo.log" 2>&1 || { cat "$out/combo.log" >&2; exit 1; }
python3 - "$out" <<'PYCODE'
import pathlib, subprocess, sys
p = pathlib.Path(sys.argv[1])
for target, origin in [('beads_bootloader', '08000000'), ('beads', '08020000')]:
    sections = subprocess.check_output(['arm-none-eabi-objdump', '-h', str(p/'objects'/target/(target+'.elf'))], text=True)
    vector = next(line.split() for line in sections.splitlines() if '.isr_vector' in line)
    if vector[3] != origin or vector[4] != origin:
        raise SystemExit('Unexpected vector VMA/LMA: '+repr(vector))
b = (p/'bootloader.bin').read_bytes()
a = (p/'app.bin').read_bytes()
if not 0 < len(b) <= 0x20000 or not a:
    raise SystemExit('Invalid payload sizes')
padded = b + bytes(0x20000-len(b))
(p/'bootloader-padded.bin').write_bytes(padded)
(p/'combo.bin').write_bytes(padded+a)
if (p/'objects/beads/beads_bootloader_combo.bin').read_bytes() != padded+a:
    raise SystemExit('Combo disagrees with upstream HEX merge')
PYCODE
(cd "$out" && sha256sum -c "$OLDPWD/beads/build/expected-hashes.sha256") || { echo "FATAL: released firmware hash mismatch" >&2; exit 1; }
(cd "$out" && sha256sum bootloader.bin bootloader-padded.bin app.bin app.elf combo.bin > SHA256SUMS
 sha256sum -c SHA256SUMS)
