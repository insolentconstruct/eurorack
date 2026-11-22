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
# Build the development blobs in the build image, from the Drizzlet root:
# the libraries first, then the models built against them.
#   beads/build/tools/build_blobs.sh OUT
set -euo pipefail
out=${1:?usage: build_blobs.sh OUT}
blob=beads/blob
speech=plaits/dsp/speech
build() {
  bash beads/build/tools/build_blob.sh "$out" "$@"
}
build plaits_wavetables $blob/libraries/plaits_wavetables.cc \
  beads/plaits_resources.cc
build speech_library $blob/libraries/speech_library.cc \
  $speech/naive_speech_synth.cc $speech/sam_speech_synth.cc \
  $speech/lpc_speech_synth.cc $speech/lpc_speech_synth_controller.cc \
  $speech/lpc_speech_synth_phonemes.cc $speech/lpc_speech_synth_words.cc \
  beads/plaits_resources.cc
build wavetable --library "$out/plaits_wavetables.bin" \
  $blob/models/wavetable.cc $blob/models/wavetable_engine.cc
build speech --library "$out/speech_library.bin" \
  $blob/models/speech.cc $blob/models/speech_engine.cc \
  $blob/libraries/speech_imports.cc
for model in particle_noise spectral_spiral phase_weave; do
  build $model $blob/models/$model.cc $blob/models/${model}_engine.cc
done
# Stock models without libraries: Plaits' helpers and tables they use.
physical=plaits/dsp/physical_modelling
for model in virtual_analog_vcf phase_distortion virtual_analog waveshaping \
    two_op_fm granular_formant harmonic swarm filtered_noise bass_drum \
    snare_drum hi_hats; do
  build $model $blob/models/$model.cc $blob/models/${model}_engine.cc \
    beads/plaits_resources.cc
done
build inharmonic_string $blob/models/inharmonic_string.cc \
  $blob/models/inharmonic_string_engine.cc $physical/string_voice.cc \
  $physical/string.cc beads/plaits_resources.cc
build chord_bank_library $blob/libraries/chord_bank_library.cc \
  plaits/dsp/chords/chord_bank.cc
build six_op_library $blob/libraries/six_op_library.cc \
  $blob/libraries/six_op_algorithms.cc beads/plaits_resources.cc
chords=(--library "$out/chord_bank_library.bin")
waves=(--library "$out/plaits_wavetables.bin")
# The voices read the opcodes too, to know the modulators.
build six_op_fm --library "$out/six_op_library.bin" $blob/models/six_op_fm.cc \
  $blob/models/six_op_fm_engine.cc $blob/libraries/six_op_algorithms.cc \
  plaits/dsp/fm/dx_units.cc beads/plaits_resources.cc
build wave_terrain "${waves[@]}" $blob/models/wave_terrain.cc \
  $blob/models/wave_terrain_engine.cc beads/plaits_resources.cc
for model in string_machine chiptune; do
  build $model "${chords[@]}" $blob/models/$model.cc \
    $blob/models/${model}_engine.cc $blob/libraries/chord_bank_imports.cc \
    beads/plaits_resources.cc
done
build chords "${waves[@]}" "${chords[@]}" $blob/models/chords.cc \
  $blob/models/chords_engine.cc $blob/libraries/chord_bank_imports.cc \
  beads/plaits_resources.cc
build modal_resonator $blob/models/modal_resonator.cc \
  $blob/models/modal_resonator_engine.cc $physical/modal_voice.cc \
  $physical/resonator.cc beads/plaits_resources.cc
# Each model's libraries are those profile-features.json lists for its ID.
python3 - "$out" <<'PYCODE'
import json, sys
models = dict((m['id'], m) for m in json.load(
    open('beads/build/profile-features.json'))['plaits_models'])
for name in ('wavetable', 'speech', 'particle_noise', 'spectral_spiral'):
  blob = json.load(open('%s/%s.bin.json' % (sys.argv[1], name)))
  listed = [d['id'] for d in blob['dependencies']]
  if listed != models[blob['model']].get('libraries', []):
    raise SystemExit('%s lists libraries %s, the catalogue %s' % (
        name, listed, models[blob['model']]['libraries']))
PYCODE
