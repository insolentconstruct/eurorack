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
# The Rings synth blob at Beads' block size (12) and at Rings' (24), the
# blob gate's Rings controls, and the emulator harnesses with a separate
# vanilla build of Rings at each block size; in the build image from the
# Drizzlet root. G1 (rings_vanilla.py) checks and writes the vanilla tree.
#   beads/test/rings_emu/build.sh OUT
set -euo pipefail
out=${1:?usage: build.sh OUT}
mkdir -p "$out/obj24" "$out/obj12" "$out/negative"
python3 beads/build/tools/rings_vanilla.py vanilla "$out/vanilla24"
mkdir -p "$out/vanilla12"
cp -r "$out/vanilla24/rings" "$out/vanilla12/"
# The reference at block 12 changes only that constant.
sed -i 's/^const size_t kMaxBlockSize = 24;$/const size_t kMaxBlockSize = 12;/' \
  "$out/vanilla12/rings/dsp/dsp.h"
grep -q '^const size_t kMaxBlockSize = 12;$' "$out/vanilla12/rings/dsp/dsp.h"

libm=beads/blob/libm
rings=(beads/blob/synths/rings.cc beads/blob/synths/rings_synth_core.cc
  rings/dsp/part.cc rings/dsp/string_synth_part.cc rings/dsp/string.cc
  rings/dsp/fm_voice.cc rings/dsp/resonator.cc rings/resources.cc
  $libm/sf_pow.c $libm/sf_pow_log2_data.c $libm/sf_exp2_data.c)
build() {
  bash beads/build/tools/build_blob.sh "$out" "$@"
}
build rings_synth -DRINGS_BLOB -DRINGS_BLOCK_SIZE=12 "${rings[@]}" \
  $libm/math_errf_no_errno.c
build rings_synth_24 -DRINGS_BLOB -DRINGS_BLOCK_SIZE=24 "${rings[@]}" \
  $libm/math_errf_no_errno.c

# Negative controls: vanilla's writable tables, and newlib's errno.
negative() {
  local name=$1 expect=$2
  shift 2
  if build "$name" "$@" > "$out/negative/$name.log" 2>&1; then
    echo "ERROR: $name: the gate accepted it"
    return 1
  elif ! grep -qE "$expect" "$out/negative/$name.log"; then
    echo "ERROR: $name: refused, but not for the expected reason"
    cat "$out/negative/$name.log"
    return 1
  fi
  echo "$name: refused: $(grep -m1 -E "$expect" "$out/negative/$name.log")"
}
negative neg_rings_writable 'GATE: .*section \.data' -DRINGS_BLOCK_SIZE=12 \
  "${rings[@]}" $libm/math_errf_no_errno.c
negative neg_rings_errno 'GATE: .*archive member lib_a-errno.o for __errno' -DRINGS_BLOB \
  -DRINGS_BLOCK_SIZE=12 "${rings[@]}" $libm/math_errf.c

unset MAKEFLAGS MFLAGS MAKELEVEL
mkdir -p "$out/make/beads"
: > "$out/make/beads/depends.mk"
M="make -f beads/build/link-order.mk KIT_MAKEFILE=beads/makefile KIT_GCC9=1 \
  BUILD_ROOT=$out/make/ -o $out/make/beads/depends.mk -s --no-print-directory"
# shellcheck disable=SC2016  # make expands $(...), not the shell.
CCFLAGS=$($M --eval 'q: ; @echo $(CCFLAGS)' q)
# shellcheck disable=SC2016
CPPFLAGS=$($M --eval 'q: ; @echo $(CPPFLAGS)' q)
# shellcheck disable=SC2016
TOOLCHAIN=$($M --eval 'q: ; @echo $(TOOLCHAIN_BIN)' q)
# The reference: vanilla Rings, linked as Rings is, with the toolchain's libm.
for block in 24 12; do
  objects=()
  tree=$out/vanilla$block
  for source in beads/test/rings_emu/harness.cc "$tree"/rings/dsp/part.cc \
      "$tree"/rings/dsp/string_synth_part.cc "$tree"/rings/dsp/string.cc \
      "$tree"/rings/dsp/fm_voice.cc "$tree"/rings/dsp/resonator.cc \
      "$tree"/rings/resources.cc beads/plaits_resources.cc \
      stmlib/dsp/units.cc stmlib/utils/random.cc; do
    object="$out/obj$block/$(basename "${source%.*}").o"
    # shellcheck disable=SC2086
    "${TOOLCHAIN}arm-none-eabi-g++" -c -I"$tree" $CCFLAGS $CPPFLAGS \
      "$source" -o "$object"
    objects+=("$object")
  done
  elf=$out/harness$block.elf
  # shellcheck disable=SC2086
  "${TOOLCHAIN}arm-none-eabi-g++" $CCFLAGS -nostartfiles --specs=nano.specs \
    -T beads/test/rings_emu/harness.ld -Wl,--gc-sections \
    -Wl,-Map="$elf.map" "${objects[@]}" -lm -lc -lgcc -o "$elf"
  "${TOOLCHAIN}arm-none-eabi-nm" "$elf" > "$out/harness$block.nm"
  # The reference really is vanilla and uses libm's powf.
  grep -q ' T powf$' "$out/harness$block.nm"
  grep -q "$tree/rings/dsp/part.cc" "$elf.map" || \
    grep -q "obj$block/part.o" "$elf.map"
done
# The blob's own symbols, for the emulator's hooks.
"${TOOLCHAIN}arm-none-eabi-nm" "$out/rings_synth/rings_synth-0x080c0000.elf" \
  > "$out/rings_synth.nm"
"${TOOLCHAIN}arm-none-eabi-nm" \
  "$out/rings_synth_24/rings_synth_24-0x080c0000.elf" > "$out/rings_synth_24.nm"
"${TOOLCHAIN}arm-none-eabi-size" -A "$out/rings_synth/rings_synth-0x080c0000.elf" \
  > "$out/rings_synth.size"
