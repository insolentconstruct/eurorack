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
libm=$blob/libm
speech=plaits/dsp/speech
build() {
  bash beads/build/tools/build_blob.sh "$out" "$@"
}
build plaits_wavetables $blob/libraries/plaits_wavetables.cc \
  beads/plaits_resources.cc
build scale_voices_library $blob/libraries/scale_voices_library.cc \
  $blob/libraries/scale_voices.cc
build wavetable_scale_voices_library --library "$out/plaits_wavetables.bin" \
  $blob/libraries/wavetable_scale_voices_library.cc \
  $blob/libraries/wavetable_scale_voices.cc
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
    loopback lockstep tapfield attractor rulefield z_filter csaw \
    ring_mod saw_square harmonics sub_oscillator digital_modulation vosim; do
  build $model $blob/models/$model.cc $blob/models/${model}_engine.cc
done
build toy $blob/models/toy.cc \
  $blob/models/toy_engine.cc beads/plaits_resources.cc
build buzz $blob/models/buzz.cc \
  $blob/models/buzz_engine.cc beads/plaits_resources.cc
build struck_bell $blob/models/struck_bell.cc \
  $blob/models/struck_bell_engine.cc beads/plaits_resources.cc
build question_mark $blob/models/question_mark.cc \
  $blob/models/question_mark_engine.cc beads/plaits_resources.cc
build bowed $blob/models/bowed.cc \
  $blob/models/bowed_engine.cc beads/plaits_resources.cc
build raw_fm $blob/models/raw_fm.cc \
  $blob/models/raw_fm_engine.cc beads/plaits_resources.cc
build triple $blob/models/triple.cc \
  $blob/models/triple_engine.cc $blob/models/triple_engine_data.cc
build bytebeat $blob/models/bytebeat.cc \
  $blob/models/bytebeat_engine.cc beads/plaits_resources.cc
build acid $blob/models/acid.cc \
  $blob/models/acid_engine.cc beads/plaits_resources.cc
build granular_cloud $blob/models/granular_cloud.cc \
  $blob/models/granular_cloud_engine.cc
build vowel_fof $blob/models/vowel_fof.cc \
  $blob/models/vowel_fof_engine.cc $blob/models/vowel_fof_data.cc
build virtual_analog_variant $blob/models/virtual_analog_variant.cc \
  $blob/models/virtual_analog_variant_engine.cc beads/plaits_resources.cc
build zxphase48k $blob/models/zxphase48k.cc \
  $blob/models/zxphase48k_engine.cc beads/plaits_resources.cc
build zxpulse48k $blob/models/zxpulse48k.cc \
  $blob/models/zxpulse48k_engine.cc $blob/models/zxpulse48k_chord_bank.cc beads/plaits_resources.cc
build gendy $blob/models/gendy.cc $blob/models/gendy_engine.cc
build dual_sync $blob/models/dual_sync.cc \
  $blob/models/dual_sync_engine.cc \
  beads/plaits_resources.cc
build morph $blob/models/morph.cc \
  $blob/models/morph_engine.cc \
  beads/plaits_resources.cc
build particle_burst $blob/models/particle_burst.cc \
  $blob/models/particle_burst_engine.cc \
  beads/plaits_resources.cc
build vowel $blob/models/vowel.cc \
  $blob/models/vowel_engine.cc \
  beads/plaits_resources.cc
build plucked $blob/models/plucked.cc \
  $blob/models/plucked_engine.cc \
  $blob/models/palette_chord_bank.cc \
  beads/plaits_resources.cc
build blown $blob/models/blown.cc \
  $blob/models/blown_engine.cc \
  beads/plaits_resources.cc
build struck_drum $blob/models/struck_drum.cc \
  $blob/models/struck_drum_engine.cc \
  beads/plaits_resources.cc
build fluted $blob/models/fluted.cc \
  $blob/models/fluted_engine.cc \
  beads/plaits_resources.cc
build shakers $blob/models/shakers.cc \
  $blob/models/shakers_engine.cc \
  beads/plaits_resources.cc
build brass $blob/models/brass.cc \
  $blob/models/brass_engine.cc \
  beads/plaits_resources.cc
build helix $blob/models/helix.cc \
  $blob/models/helix_engine.cc \
  $blob/models/palette_chord_bank.cc \
  beads/plaits_resources.cc
build skins $blob/models/skins.cc \
  $blob/models/skins_engine.cc \
  beads/plaits_resources.cc
build circuit_zaps $blob/models/circuit_zaps.cc \
  $blob/models/circuit_zaps_engine.cc \
  beads/plaits_resources.cc
build metalwork $blob/models/metalwork.cc \
  $blob/models/metalwork_engine.cc \
  beads/plaits_resources.cc
build clap $blob/models/clap.cc \
  $blob/models/clap_engine.cc \
  beads/plaits_resources.cc
build analog_percussion $blob/models/analog_percussion.cc \
  $blob/models/analog_percussion_engine.cc \
  beads/plaits_resources.cc
build saw_swarm $blob/models/saw_swarm.cc \
  $blob/models/saw_swarm_engine.cc \
  beads/plaits_resources.cc
build bubbletime $blob/models/bubbletime.cc \
  $blob/models/bubbletime_engine.cc \
  $blob/models/palette_chord_bank.cc \
  beads/plaits_resources.cc
build chords_supersaw $blob/models/chords_supersaw.cc \
  $blob/models/chords_supersaw_engine.cc \
  $blob/models/palette_chord_bank.cc \
  beads/plaits_resources.cc
build fold $blob/models/fold.cc $blob/models/fold_engine.cc \
  beads/plaits_resources.cc
build noise_bank $blob/models/noise_bank.cc $blob/models/noise_bank_engine.cc \
  $libm/sinf.c $libm/sincosf_data.c $libm/math_errf_no_errno.c \
  $libm/libgcc/convert.S
build kick $blob/models/kick.cc $blob/models/kick_engine.cc \
  $libm/sinf.c $libm/sincosf_data.c $libm/math_errf_no_errno.c \
  $libm/libgcc/convert.S
build snare $blob/models/snare.cc $blob/models/snare_engine.cc \
  $libm/sinf.c $libm/sincosf_data.c $libm/math_errf_no_errno.c \
  $libm/libgcc/convert.S
build cymbal $blob/models/cymbal.cc $blob/models/cymbal_engine.cc \
  $libm/sinf.c $libm/sincosf_data.c $libm/math_errf_no_errno.c \
  $libm/libgcc/convert.S
build wave_paraphonic --library "$out/plaits_wavetables.bin" \
  $blob/models/wave_paraphonic.cc $blob/models/wave_paraphonic_engine.cc
build saw_comb $blob/models/saw_comb.cc $blob/models/saw_comb_engine.cc \
  $libm/sf_tanh.c $libm/sf_expm1.c $libm/math_errf_no_errno.c
build diatonic_chord --library "$out/scale_voices_library.bin" \
  $blob/models/diatonic_chord.cc $blob/models/diatonic_chord_engine.cc \
  $blob/libraries/scale_voices_imports.cc
build scale_stack --library "$out/scale_voices_library.bin" \
  $blob/models/scale_stack.cc $blob/models/scale_stack_engine.cc \
  $blob/libraries/scale_voices_imports.cc
build wavetable_chord --library "$out/plaits_wavetables.bin" \
  --library "$out/scale_voices_library.bin" \
  --library "$out/wavetable_scale_voices_library.bin" \
  $blob/models/wavetable_chord.cc $blob/models/wavetable_chord_engine.cc \
  $blob/libraries/scale_voices_imports.cc \
  $blob/libraries/wavetable_scale_voices_imports.cc
build wavetable_scale_stack --library "$out/plaits_wavetables.bin" \
  --library "$out/scale_voices_library.bin" \
  --library "$out/wavetable_scale_voices_library.bin" \
  $blob/models/wavetable_scale_stack.cc $blob/models/wavetable_scale_stack_engine.cc \
  $blob/libraries/scale_voices_imports.cc \
  $blob/libraries/wavetable_scale_voices_imports.cc
build freshets_formant $blob/models/freshets_formant.cc \
  $blob/models/freshets_formant_engine.cc beads/plaits_resources.cc
build speech_sounds --library "$out/speech_library.bin" \
  $blob/models/speech_sounds.cc \
  $blob/models/speech_sounds_engine.cc \
  $blob/models/speech_sounds_controller.cc \
  $blob/models/speech_sounds_lpc.cc \
  $blob/models/speech_sounds_phonemes.cc \
  $blob/libraries/speech_imports.cc beads/plaits_resources.cc
build lpc_words --library "$out/speech_library.bin" \
  $blob/models/lpc_words.cc \
  $blob/models/lpc_words_engine.cc \
  $blob/models/lpc_words_controller.cc \
  $blob/models/lpc_words_synth.cc \
  $blob/models/lpc_words_word_bank.cc \
  $blob/libraries/speech_imports.cc beads/plaits_resources.cc
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
