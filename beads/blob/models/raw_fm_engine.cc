// Copyright 2012 Emilie Gillet.
// Copyright 2026 Lyle Mills.
// Copyright 2026 Insolent Construct.
// SPDX-License-Identifier: MIT
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
//
// RawFm: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/raw_fm_engine.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"
#include "plaits/resources.h"

namespace plaits {

using namespace std;
using namespace stmlib;

void RawFm::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void RawFm::Reset() {
  phase_ = 0.0f;
  modulator_phase_ = 0.0f;
  previous_sample_ = 0.0f;
}

void RawFm::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    phase_ = 0.0f;
    modulator_phase_ = 0.0f;
    previous_sample_ = 0.0f;
  }

  const float frequency = NoteToFrequency(parameters.note);
  const float ratio = Interpolate(
      lut_fm_frequency_quantizer, parameters.harmonics, 128.0f);
  float modulator_note = parameters.note + ratio;
  if (modulator_note > kRawFmModulatorNoteCeiling) {
    modulator_note = kRawFmModulatorNoteCeiling;
  }
  const float modulator_frequency = min(
      NoteToFrequency(modulator_note), 0.5f);
  float attenuation = (32767.0f - kRawFmAttenuatorSlope *
      (parameters.note - kRawFmAttenuatorPivotNote + ratio)) * (1.0f / 32768.0f);
  CONSTRAIN(attenuation, 0.0f, kRawFmAttenuatorFull);
  const float centre = 1.0f - fabsf(2.0f * parameters.morph - 1.0f);
  const float phase_feedback = max(1.0f - 2.0f * parameters.morph, 0.0f);
  const float frequency_feedback = max(2.0f * parameters.morph - 1.0f, 0.0f);
  const float depth = 1.0f;
  const float index = parameters.timbre *
      (kRawFmEdgeIndex + (kRawFmCentreIndex - kRawFmEdgeIndex) * centre) *
      (1.0f + (attenuation - 1.0f) * phase_feedback);
  const float modulator_ratio = modulator_frequency /
      (frequency > 1.0e-9f ? frequency : 1.0e-9f);

  for (size_t i = 0; i < size; ++i) {
    const float root_offset = 0.0f;
    float carrier_frequency = frequency + root_offset;
    CONSTRAIN(carrier_frequency, -0.5f, 0.499999f);
    phase_ += carrier_frequency;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
    } else if (phase_ < 0.0f) {
      phase_ += 1.0f;
    }
    const float centre_ratio = 1.0f +
        (kRawFmWtfmCentre - 1.0f) * frequency_feedback;
    float feedback_steps = floorf(previous_sample_ * kRawFmWtfmFeedbackSteps);
    CONSTRAIN(feedback_steps, -kRawFmWtfmFeedbackSteps,
        kRawFmWtfmFeedbackSteps - 1.0f);
    float increment = (modulator_frequency + root_offset * modulator_ratio) *
        (centre_ratio +
        kRawFmFrequencyFeedback * frequency_feedback * depth *
        feedback_steps * (1.0f / kRawFmWtfmFeedbackSteps));
    CONSTRAIN(increment, -0.5f, 0.499999f);
    modulator_phase_ += increment;
    if (modulator_phase_ >= 1.0f) {
      modulator_phase_ -= 1.0f;
    } else if (modulator_phase_ < 0.0f) {
      modulator_phase_ += 1.0f;
    }
    const float modulator = Sine(modulator_phase_ + kRawFmPhaseOffset +
        kRawFmPhaseFeedback * phase_feedback * depth * previous_sample_);

    previous_sample_ = Sine(phase_ + kRawFmPhaseOffset + index * modulator);
    out[i] = previous_sample_;
    aux[i] = modulator;
  }
}

}  // namespace plaits
