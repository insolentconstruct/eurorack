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
