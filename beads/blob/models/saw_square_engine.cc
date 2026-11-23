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
// SawSquare: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/saw_square_engine.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/polyblep.h"

namespace plaits {
using namespace std;
using namespace stmlib;

void SawSquare::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void SawSquare::Reset() {
  phase_ = 0.0f;
  frequency_ = 0.01f;
  pw_saw_ = 0.25f;
  previous_pw_saw_ = 0.25f;
  high_saw_ = false;
  next_sample_saw_ = 0.0f;

  phase_square_ = 0.0f;
  pw_square_ = 0.25f;
  previous_pw_square_ = 0.25f;
  high_square_ = false;
  next_sample_square_ = 0.0f;

  offset_ = 0.0f;
  atten_ = kSawSquareAttenuationStock;
  blend_ = 0.0f;
}

void SawSquare::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  const float target_offset =
      (parameters.morph - 0.5f) * kSawSquareMaxPhaseOffset;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    phase_ = 0.0f;
    phase_square_ = target_offset;
    if (phase_square_ < 0.0f) {
      phase_square_ += 1.0f;
    } else if (phase_square_ >= 1.0f) {
      phase_square_ -= 1.0f;
    }
    high_saw_ = false;
    high_square_ = false;
    next_sample_saw_ = 0.0f;
    next_sample_square_ = 0.0f;
  } else {
    phase_square_ += (target_offset - offset_);
    if (phase_square_ >= 1.0f) {
      phase_square_ -= 1.0f;
    } else if (phase_square_ < 0.0f) {
      phase_square_ += 1.0f;
    }
  }
  offset_ = target_offset;

  const float frequency = NoteToFrequency(parameters.note);

  float target_pw_saw = 0.5f * parameters.timbre;
  float target_pw_square = 0.5f * (1.0f - parameters.timbre);
  CONSTRAIN(target_pw_saw, kSawSquareSawPwMin, 0.5f);
  CONSTRAIN(target_pw_square, kSawSquareSquarePwMin, 0.5f);

  if (frequency >= 0.25f) {
    target_pw_saw = 0.5f;
    target_pw_square = 0.5f;
  } else {
    CONSTRAIN(target_pw_saw, 2.0f * frequency, 1.0f - 2.0f * frequency);
    CONSTRAIN(target_pw_square, 2.0f * frequency, 1.0f - 2.0f * frequency);
  }

  const float target_atten = kSawSquareAttenuationStock;

  ParameterInterpolator fm(&frequency_, frequency, size);

  ParameterInterpolator blend_modulation(
      &blend_, parameters.harmonics, size);
  ParameterInterpolator pw_saw_modulation(&pw_saw_, target_pw_saw, size);
  ParameterInterpolator pw_square_modulation(
      &pw_square_, target_pw_square, size);
  ParameterInterpolator atten_modulation(&atten_, target_atten, size);

  float next_sample_saw = next_sample_saw_;
  float next_sample_square = next_sample_square_;

  for (size_t i = 0; i < size; ++i) {
    float this_sample_saw = next_sample_saw;
    float this_sample_square = next_sample_square;
    next_sample_saw = 0.0f;
    next_sample_square = 0.0f;

    float f = fm.Next();
    const float pw_saw = pw_saw_modulation.Next();
    const float pw_square = pw_square_modulation.Next();
    const float atten = atten_modulation.Next();
    const float blend = blend_modulation.Next();

    phase_ += f;
    while (true) {
      if (!high_saw_) {
        if (phase_ < pw_saw) {
          break;
        }
        float t = (phase_ - pw_saw) / (previous_pw_saw_ - pw_saw + f);
        CONSTRAIN(t, 0.0f, 1.0f);
        this_sample_saw -= ThisBlepSample(t);
        next_sample_saw -= NextBlepSample(t);
        high_saw_ = true;
      }
      if (high_saw_) {
        if (phase_ < 1.0f) {
          break;
        }
        phase_ -= 1.0f;
        float t = phase_ / f;
        CONSTRAIN(t, 0.0f, 1.0f);
        this_sample_saw -= ThisBlepSample(t);
        next_sample_saw -= NextBlepSample(t);
        high_saw_ = false;
      }
    }

    next_sample_saw += (phase_ < pw_saw)
        ? (2.0f * phase_ - pw_saw)
        : (2.0f * phase_ - pw_saw - 1.0f);
    previous_pw_saw_ = pw_saw;

    phase_square_ += f;
    while (true) {
      if (!high_square_) {
        if (phase_square_ < pw_square) {
          break;
        }
        float t = (phase_square_ - pw_square) /
            (previous_pw_square_ - pw_square + f);
        CONSTRAIN(t, 0.0f, 1.0f);
        this_sample_square += ThisBlepSample(t);
        next_sample_square += NextBlepSample(t);
        high_square_ = true;
      }
      if (high_square_) {
        if (phase_square_ < 1.0f) {
          break;
        }
        phase_square_ -= 1.0f;
        float t = phase_square_ / f;
        CONSTRAIN(t, 0.0f, 1.0f);
        this_sample_square -= ThisBlepSample(t);
        next_sample_square -= NextBlepSample(t);
        high_square_ = false;
      }
    }

    next_sample_square += (phase_square_ < pw_square) ? 0.0f : 1.0f;
    previous_pw_square_ = pw_square;

    const float saw = this_sample_saw;
    const float square = 2.0f * this_sample_square - 1.0f;
    const float attenuated_square = square * atten;

    float blend_main = blend;
    float blend_aux = 1.0f - blend;

    out[i] = saw + (attenuated_square - saw) * blend_main;
    aux[i] = saw + (attenuated_square - saw) * blend_aux;
  }

  next_sample_saw_ = next_sample_saw;
  next_sample_square_ = next_sample_square;
}

}  // namespace plaits
