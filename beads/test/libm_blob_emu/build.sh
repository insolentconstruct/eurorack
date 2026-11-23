#!/usr/bin/env bash
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
# See http://creativecommons.org/licenses/MIT/ for more information.
#
# Build a libm probe, its errno negative and a compiled-in reference.
set -euo pipefail
out=${1:?usage: build.sh OUT}
mkdir -p "$out/make/beads"
: > "$out/make/beads/depends.mk"
M="make -f beads/build/link-order.mk KIT_MAKEFILE=beads/makefile KIT_GCC9=1 BUILD_ROOT=$out/make/ -o $out/make/beads/depends.mk -s --no-print-directory"
CCFLAGS=$($M --eval 'q: ; @echo $(CCFLAGS)' q)
TOOLCHAIN=$($M --eval 'q: ; @echo $(TOOLCHAIN_BIN)' q)
libm=beads/blob/libm
sources=(beads/test/libm_blob_emu/probe.cc "$libm/sinf.c"
  "$libm/sincosf_data.c" "$libm/sf_tanh.c" "$libm/sf_expm1.c"
  "$libm/libgcc/convert.S")
bash beads/build/tools/build_blob.sh "$out" libm_probe "${sources[@]}" \
  "$libm/math_errf_no_errno.c"
pins=$PWD/beads/test/libm_blob_emu/expected.sha256
(cd "$out" && sha256sum -c "$pins")
if bash beads/build/tools/build_blob.sh "$out" libm_errno "${sources[@]}" \
    "$libm/math_errf.c" > "$out/errno.log" 2>&1; then
  echo 'ERROR: errno control accepted'
  exit 1
fi
grep -q 'GATE: .*archive member lib_a-errno.o for __errno' "$out/errno.log"
# shellcheck disable=SC2086
"${TOOLCHAIN}arm-none-eabi-gcc" $CCFLAGS -c \
  beads/test/libm_blob_emu/harness.c -o "$out/harness.o"
# shellcheck disable=SC2086
"${TOOLCHAIN}arm-none-eabi-gcc" $CCFLAGS -nostartfiles \
  -T beads/test/libm_blob_emu/harness.ld -Wl,--gc-sections \
  -Wl,-Map="$out/harness.map" "$out/harness.o" -lm -lc -lgcc \
  -o "$out/harness.elf"
"${TOOLCHAIN}arm-none-eabi-nm" "$out/harness.elf" > "$out/harness.nm"
"${TOOLCHAIN}arm-none-eabi-objdump" -dr "$out/harness.elf" > "$out/harness.dis"
for symbol in sinf tanhf expm1f; do
  grep -q " T $symbol\$" "$out/harness.nm"
done
