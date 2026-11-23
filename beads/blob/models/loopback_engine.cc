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
// Loopback: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/loopback_engine.h"

#include <algorithm>
#include <cmath>

#include "plaits/dsp/oscillator/sine_oscillator.h"
#include "stmlib/dsp/parameter_interpolator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

void Loopback::Init(BufferAllocator* allocator) {
  Reset();
}

void Loopback::Reset() {
  carrier_phase_ = 0.0f;
  feedback_phase_ = 0.0f;
  feedback_state_ = 0.0f;
  fb_osc_ = 0.0f;
  ratio_ = 1.0f;
  depth_ = 0.0f;
  morph_ = 0.0f;
  polarity_ = 0.0f;
}

void Loopback::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    feedback_state_ = 0.0f;
    fb_osc_ = 0.0f;
    carrier_phase_ = 0.0f;
    feedback_phase_ = 0.0f;
  }

  const float frequency = min(0.24f, NoteToFrequency(parameters.note));
  const float ratio = 0.5f + 7.5f *
      parameters.harmonics * parameters.harmonics;
  const float depth = 0.98f * parameters.timbre * parameters.timbre;
  const float polarity = 2.0f * 0.5f - 1.0f;

  const float morph = kLoopbackFeedbackDepth * parameters.morph;

  ParameterInterpolator ratio_modulation(&ratio_, ratio, size);
  ParameterInterpolator depth_modulation(&depth_, depth, size);
  ParameterInterpolator morph_modulation(&morph_, morph, size);
  ParameterInterpolator polarity_modulation(&polarity_, polarity, size);

  for (size_t i = 0; i < size; ++i) {
    const float current_ratio = ratio_modulation.Next();
    const float current_depth = depth_modulation.Next();
    const float current_morph = morph_modulation.Next();
    const float current_polarity = polarity_modulation.Next();

    float root_frequency = frequency;
    CONSTRAIN(root_frequency, -0.24f, 0.24f);
    float feedback_frequency = root_frequency * current_ratio;
    CONSTRAIN(feedback_frequency, -0.24f, 0.24f);
    carrier_phase_ += root_frequency;
    if (carrier_phase_ >= 1.0f) {
      carrier_phase_ -= 1.0f;
    } else if (carrier_phase_ < 0.0f) {
      carrier_phase_ += 1.0f;
    }
    feedback_phase_ += feedback_frequency;
    if (feedback_phase_ >= 1.0f) {
      feedback_phase_ -= 1.0f;
    } else if (feedback_phase_ < 0.0f) {
      feedback_phase_ += 1.0f;
    }

    const float carrier = SineNoWrap(carrier_phase_);
    float modulated_phase = feedback_phase_ + current_morph * fb_osc_;
    modulated_phase -= floorf(modulated_phase);
    const float feedback_carrier = SineNoWrap(modulated_phase);
    fb_osc_ = feedback_carrier;

    const float magnitude = fabsf(feedback_state_);
    float shaped_feedback;
    if (current_polarity < 0.0f) {
      shaped_feedback = feedback_state_ + (-magnitude - feedback_state_) *
          -current_polarity;
    } else {
      shaped_feedback = feedback_state_ + (magnitude - feedback_state_) *
          current_polarity;
    }

    const float normalization = 1.0f / (1.0f + current_depth);
    const float envelope = (1.0f +
        current_depth * shaped_feedback) * normalization;
    float recursive = feedback_carrier * envelope;
    CONSTRAIN(recursive, -1.0f, 1.0f);
    feedback_state_ = recursive;

    const float main = carrier * envelope;
    const float sidebands = carrier * current_depth *
        shaped_feedback * normalization;
    out[i] = 0.9f * main;
    aux[i] = 1.55f * sidebands;
  }
}

}  // namespace plaits
