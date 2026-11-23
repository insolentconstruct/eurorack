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
# the libraries first, then the models built against them, then the Rings
# synth.
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
for model in particle_noise spectral_spiral phase_weave pulsar glisson \
    reed_pipe sideband undertow phase_flock scanned \
    loopback lockstep; do
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
  $blob/models/inharmonic_string_engine.cc $blob/models/blob_string_voice.cc \
  $blob/models/blob_string.cc beads/plaits_resources.cc
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
# The Rings synth at Beads' block size, as beads/test/rings_emu/build.sh
# builds it.
libm=$blob/libm
build rings_synth -DRINGS_BLOB -DRINGS_BLOCK_SIZE=12 $blob/synths/rings.cc \
  $blob/synths/rings_synth_core.cc rings/dsp/part.cc \
  rings/dsp/string_synth_part.cc rings/dsp/string.cc rings/dsp/fm_voice.cc \
  rings/dsp/resonator.cc rings/resources.cc $libm/sf_pow.c \
  $libm/sf_pow_log2_data.c $libm/sf_exp2_data.c $libm/math_errf_no_errno.c
# Each model's libraries are those profile-features.json lists for its ID.
python3 beads/build/tools/blob_libraries.py "$out"
