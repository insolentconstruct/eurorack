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
# Build the emulator harness, the blobs and their region (WAVE and PLTS), in
# the build image from the Drizzlet root; then the services link's negative
# controls.
#   beads/test/blob_emu/build.sh OUT
set -euo pipefail
out=${1:?usage: build.sh OUT}
mkdir -p "$out/obj" "$out/negative"
models=beads/blob/models
bash beads/build/tools/build_blobs.sh "$out/blob"
bash beads/build/tools/build_blob.sh "$out/blob" pos_sine \
  beads/test/blob_gate/pos_sine.cc
blobs=("$out/blob/plaits_wavetables.bin" "$out/blob/speech_library.bin" \
  "$out/blob/phase_weave.bin" "$out/blob/pos_sine.bin" \
  "$out/blob/particle_noise.bin" "$out/blob/spectral_spiral.bin" \
  "$out/blob/wavetable.bin" "$out/blob/speech.bin")
python3 beads/build/tools/blob_sections.py "$out/region.bin" 20261121 \
  --wave beads/resources.cc "${blobs[@]}"
# The same with the models 0x2fe0 further from the libraries: with the
# region's two placements, libraries and dependents each move independently.
python3 beads/build/tools/blob_sections.py "$out/region-gap.bin" 20261121 \
  --gap 0x2fe0 --wave beads/resources.cc "${blobs[@]}"

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
compile() {
  # shellcheck disable=SC2086
  "${TOOLCHAIN}arm-none-eabi-g++" -c $CCFLAGS $CPPFLAGS "$1" -o "$2"
}
link() {
  local elf=$1
  shift
  # shellcheck disable=SC2086
  "${TOOLCHAIN}arm-none-eabi-g++" $CCFLAGS -nostartfiles --specs=nano.specs \
    -T beads/test/blob_emu/harness.ld -Wl,--gc-sections -Wl,-Map="$elf.map" \
    "$@" -lm -lc -lgcc -o "$elf"
}
objects=()
speech=plaits/dsp/speech
for source in beads/test/blob_emu/harness.cc beads/blob/plaits_blob.cc \
    beads/blob/plaits_region.cc beads/blob/plaits_libraries.cc \
    $models/phase_weave_engine.cc $models/spectral_spiral_engine.cc \
    plaits/dsp/engine/particle_engine.cc plaits/dsp/engine/wavetable_engine.cc \
    plaits/dsp/engine/speech_engine.cc $speech/naive_speech_synth.cc \
    $speech/sam_speech_synth.cc $speech/lpc_speech_synth.cc \
    $speech/lpc_speech_synth_controller.cc \
    $speech/lpc_speech_synth_phonemes.cc $speech/lpc_speech_synth_words.cc \
    beads/plaits_resources.cc beads/profile.cc beads/dsp/pitch_quantizer.cc \
    stmlib/utils/random.cc; do
  object="$out/obj/$(basename "${source%.*}").o"
  compile "$source" "$object"
  objects+=("$object")
done
compile stmlib/dsp/units.cc "$out/obj/units.o"
link "$out/harness.elf" "${objects[@]}" "$out/obj/units.o"
"${TOOLCHAIN}arm-none-eabi-nm" "$out/harness.elf" > "$out/harness.nm"
# The exports' own symbols, not only their sections: address and size.
"${TOOLCHAIN}arm-none-eabi-nm" -S "$out/harness.elf" | awk '
  $4 == "_ZN6plaits8lut_sineE" { s = s $1 ":" $2 " " }
  $4 == "_ZN6stmlib20lut_pitch_ratio_highE" { h = $1 ":" $2 }
  $4 == "_ZN6stmlib19lut_pitch_ratio_lowE" { l = $1 ":" $2 }
  $4 == "_ZN6stmlib6Random10rng_state_E" { r = $1 ":" $2 }
  $4 == "_ZN6plaits8lut_foldE" { f = $1 ":" $2 }
  $4 == "_ZN6plaits26lut_fm_frequency_quantizerE" { q = $1 ":" $2 }
  $4 == "_ZN6plaits22lut_4x_downsampler_firE" { d = $1 ":" $2 }
  $4 == "_ZN6plaits18lut_ws_inverse_tanE" { t = $1 ":" $2 }
  $4 == "_ZN6plaits18lut_ws_inverse_sinE" { n = $1 ":" $2 }
  $4 == "_ZN6plaits13lut_ws_linearE" { i = $1 ":" $2 }
  $4 == "_ZN6plaits11lut_ws_bumpE" { b = $1 ":" $2 }
  END {
    got = s h " " l " " r " " f " " q " " d " " t " " n " " i " " b
    want = "08021440:00000a04 08021e44:00000404 08022248:00000404 " \
        "20000000:00000004 0802264c:00000810 08022e5c:00000208 " \
        "08023064:00000010 08023074:00000202 08023278:00000202 " \
        "0802347c:00000202 08023680:00000202"
    if (got != want) { print "ERROR: export symbols " got; exit 1 }
    print "export symbols: " got
  }'
"${TOOLCHAIN}arm-none-eabi-objdump" -h "$out/harness.elf" \
  | grep -E 'blob_(services|lut)' | tee "$out/services.txt"

# Each negative replaces stmlib's pitch tables and must fail at link.
failures=0
for source in beads/test/blob_emu/neg_*.cc; do
  name=$(basename "$source" .cc)
  expect=$(sed -n 's|^// Expect: ||p' "$source")
  compile "$source" "$out/negative/$name.o"
  if link "$out/negative/$name.elf" "${objects[@]}" \
      "$out/negative/$name.o" > "$out/negative/$name.log" 2>&1; then
    echo "ERROR: $name: the link accepted it"
    failures=$((failures + 1))
  elif [ -z "$expect" ] || ! grep -qE "$expect" "$out/negative/$name.log"; then
    echo "ERROR: $name: refused, but not for the expected reason"
    cat "$out/negative/$name.log"
    failures=$((failures + 1))
  else
    echo "$name: refused: $(grep -m1 -E "$expect" "$out/negative/$name.log")"
  fi
done
exit $((failures > 0))
