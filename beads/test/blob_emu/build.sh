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
# Build the emulator harness, the blobs and their region (WAVE, PLTS and
# RNGS), in the build image from the Drizzlet root, with the library check's
# negative controls; then the services link's.
#   beads/test/blob_emu/build.sh OUT
set -euo pipefail
out=${1:?usage: build.sh OUT}
mkdir -p "$out/obj" "$out/negative"
models=beads/blob/models
bash beads/build/tools/build_blobs.sh "$out/blob"
# The library check's controls: each wrong catalogue entry must be refused,
# 6-op FM's later IDs included.
for control in '3:=six_op_fm lists' '14:1=chords lists' '14:2,1=chords lists' \
    '8:1=virtual_analog lists' 'library:2=library chord_bank_library has ID'; do
  wrong=${control%%=*}
  python3 - "$wrong" "$out/negative/catalogue.json" <<'PYCODE'
import json, sys
model, ids = sys.argv[1].split(':')
catalogue = json.load(open('beads/build/profile-features.json'))
if model == 'library':
  catalogue['plaits_libraries'] = [l for l in catalogue['plaits_libraries']
                                   if l['id'] != int(ids)]
for m in catalogue['plaits_models']:
  if str(m['id']) == model:
    m['libraries'] = [int(i) for i in ids.split(',') if i]
json.dump(catalogue, open(sys.argv[2], 'w'))
PYCODE
  if python3 beads/build/tools/blob_libraries.py "$out/blob" \
      "$out/negative/catalogue.json" 2> "$out/negative/catalogue.log"; then
    echo "ERROR: libraries $wrong: the check accepted it"
    exit 1
  elif ! grep -q "^LIBRARIES: ${control#*=}" "$out/negative/catalogue.log"; then
    echo "ERROR: libraries $wrong: refused, but not for the expected reason"
    cat "$out/negative/catalogue.log"
    exit 1
  fi
  echo "libraries $wrong: refused: $(head -n1 "$out/negative/catalogue.log")"
done
bash beads/build/tools/build_blob.sh "$out/blob" pos_sine \
  -DTEST_SINE_MODEL=254 beads/test/blob_gate/pos_sine.cc
# Every blob: the stock models and their libraries, the proof's models, the
# Rings synth (RNGS).
blobs=("$out"/blob/*.bin)
python3 beads/build/tools/blob_sections.py "$out/region.bin" 20261121 \
  --wave beads/resources.cc "${blobs[@]}"
# One deterministic user terrain, alongside the unchanged stock region.
python3 - "$out/terrain.bin" <<'PYCODE'
import math, sys
grid = bytes(round(127 * math.sin(0.19 * x + 0.11 * y +
                                0.3 * math.sin(0.05 * x * y))) & 255
             for y in range(64) for x in range(64))
open(sys.argv[1], 'wb').write(grid)
PYCODE
python3 beads/build/tools/blob_sections.py "$out/region-terr.bin" 20261121 \
  --wave beads/resources.cc --terrain "$out/terrain.bin" "${blobs[@]}"
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
  "${TOOLCHAIN}arm-none-eabi-g++" -c $CCFLAGS $CPPFLAGS "$1" -o "$2" "${@:3}"
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
physical=plaits/dsp/physical_modelling
# The compiled-in references: every stock engine and its helpers.
for source in beads/test/blob_emu/harness.cc beads/blob/plaits_blob.cc \
    beads/blob/plaits_region.cc beads/blob/plaits_libraries.cc \
    $models/phase_weave_engine.cc $models/spectral_spiral_engine.cc \
    $models/pulsar_engine.cc $models/glisson_engine.cc $models/reed_pipe_engine.cc \
    $models/sideband_engine.cc $models/undertow_engine.cc $models/phase_flock_engine.cc \
    $models/scanned_engine.cc $models/gendy_engine.cc \
    $models/particle_burst_engine.cc \
    $models/struck_drum_engine.cc \
    $models/plucked_engine.cc \
    $models/palette_chord_bank.cc \
    $models/blown_engine.cc \
    $models/fluted_engine.cc \
    plaits/dsp/engine/*.cc plaits/dsp/engine2/*.cc \
    plaits/dsp/chords/chord_bank.cc plaits/dsp/fm/algorithms.cc \
    plaits/dsp/fm/dx_units.cc $physical/modal_voice.cc $physical/resonator.cc \
    $physical/string.cc $physical/string_voice.cc \
    $speech/naive_speech_synth.cc \
    $speech/sam_speech_synth.cc $speech/lpc_speech_synth.cc \
    $speech/lpc_speech_synth_controller.cc \
    $speech/lpc_speech_synth_phonemes.cc $speech/lpc_speech_synth_words.cc \
    beads/plaits_resources.cc beads/profile.cc beads/dsp/pitch_quantizer.cc \
    stmlib/utils/random.cc; do
  object="$out/obj/$(basename "${source%.*}").o"
  if [ "$source" = plaits/dsp/engine2/wave_terrain_engine.cc ]; then
    # Counts the lookups that leave the bank, where the blob clamps.
    compile "$source" "$object" -include beads/test/blob_emu/terrain_probe.h
  else
    compile "$source" "$object"
  fi
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
