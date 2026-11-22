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
#
# The release's Plaits region, in the build image from the Drizzlet root: the
# blobs, the default region (the stock models and their libraries), and the
# catalogue, which must equal beads/blob/plaits_catalog.cc (its digests
# approve every blob built here).
#   beads/build/tools/build_region.sh OUT
set -euo pipefail
out=${1:?usage: build_region.sh OUT}
mkdir -p "$out"
release=$(sed -n 's/^const uint32_t kPlaitsBlobRelease = \([0-9]*\);$/\1/p' \
  beads/blob/plaits_catalog.cc)
bash beads/build/tools/build_blobs.sh "$out/blob" > "$out/blob.log" 2>&1 \
  || { tail -40 "$out/blob.log" >&2; exit 1; }
blob=$out/blob
stock=("$blob/plaits_wavetables.bin" "$blob/chord_bank_library.bin" \
  "$blob/speech_library.bin" "$blob/six_op_library.bin" \
  "$blob/virtual_analog_vcf.bin" "$blob/phase_distortion.bin" \
  "$blob/six_op_fm.bin" "$blob/wave_terrain.bin" "$blob/string_machine.bin" \
  "$blob/chiptune.bin" "$blob/virtual_analog.bin" "$blob/waveshaping.bin" \
  "$blob/two_op_fm.bin" "$blob/granular_formant.bin" "$blob/harmonic.bin" \
  "$blob/wavetable.bin" "$blob/chords.bin" "$blob/speech.bin" \
  "$blob/swarm.bin" "$blob/filtered_noise.bin" "$blob/particle_noise.bin" \
  "$blob/inharmonic_string.bin" "$blob/modal_resonator.bin" \
  "$blob/bass_drum.bin" "$blob/snare_drum.bin" "$blob/hi_hats.bin")
python3 beads/build/tools/blob_sections.py "$out/region.bin" "$release" \
  "${stock[@]}"
python3 beads/build/tools/blob_catalog.py "$out/plaits_catalog.cc" \
  "$release" "$out/region.bin.json" "$blob"/*.bin
if ! cmp -s beads/blob/plaits_catalog.cc "$out/plaits_catalog.cc"; then
  diff beads/blob/plaits_catalog.cc "$out/plaits_catalog.cc" || true
  echo "beads/blob/plaits_catalog.cc is stale: copy $out/plaits_catalog.cc"
  exit 1
fi
