// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Mutable Instruments' Rings as a synth: Part, StringSynthPart and Strummer.

#include "beads/blob/synths/rings_synth_core.h"

#include <algorithm>

namespace beads {

using namespace std;
using namespace rings;

void RingsSynthCore::Init() {
  // As rings.cc's Init.
  strummer_.Init(0.01f, kSampleRate / kMaxBlockSize);
  part_.Init(reverb_buffer_);
  string_synth_.Init(reverb_buffer_);
  target_.core = RINGS_CORE_PART;
  target_.model = RESONATOR_MODEL_MODAL;
  target_.polyphony = 1;
  target_.fx = FX_ENSEMBLE;
}

/* static */
bool RingsSynthCore::Valid(const RingsSynthTarget& target) {
  return target.core < RINGS_CORE_LAST &&
      target.model < RESONATOR_MODEL_LAST &&
      (target.polyphony == 1 || target.polyphony == 2 ||
       target.polyphony == 4) &&
      target.fx < FX_LAST;
}

bool RingsSynthCore::Configure(
    const RingsSynthTarget& target,
    const RingsSynthControls& controls,
    bool entry) {
  if (!Valid(target)) {
    return false;
  }
  // As Rings' UI: both cores take the polyphony; set_polyphony always
  // reconfigures Part, so it is called only on a change.
  if (target.polyphony != target_.polyphony) {
    part_.set_polyphony(target.polyphony);
    string_synth_.set_polyphony(target.polyphony);
  }
  part_.set_model(static_cast<ResonatorModel>(target.model));
  string_synth_.set_fx(static_cast<FxType>(target.fx));
  target_ = target;
  if (entry) {
    Prime(RINGS_CORE_PART, controls);
    Prime(RINGS_CORE_STRING_SYNTH, controls);
  } else {
    Prime(target.core, controls);
  }
  return true;
}

void RingsSynthCore::Prime(uint8_t core, const RingsSynthControls& controls) {
  float in[kMaxBlockSize];
  float out[kMaxBlockSize];
  float aux[kMaxBlockSize];
  fill(&in[0], &in[kMaxBlockSize], 0.0f);
  PerformanceState performance_state = controls.performance_state;
  performance_state.strum = false;
  if (core == RINGS_CORE_STRING_SYNTH) {
    string_synth_.Process(
        performance_state, controls.patch, in, out, aux, kMaxBlockSize);
  } else {
    part_.Process(
        performance_state, controls.patch, in, out, aux, kMaxBlockSize);
  }
}

void RingsSynthCore::Render(
    RingsSynthControls* controls,
    float* out,
    float* aux,
    size_t size) {
  // Synth mode has both audio inputs unpatched: the exciter is internal.
  float in[kMaxBlockSize];
  fill(&in[0], &in[size], 0.0f);
  PerformanceState* performance_state = &controls->performance_state;
  if (target_.core == RINGS_CORE_STRING_SYNTH) {
    strummer_.Process(NULL, size, performance_state);
    string_synth_.Process(
        *performance_state, controls->patch, in, out, aux, size);
  } else {
    strummer_.Process(in, size, performance_state);
    part_.Process(*performance_state, controls->patch, in, out, aux, size);
  }
}

}  // namespace beads
