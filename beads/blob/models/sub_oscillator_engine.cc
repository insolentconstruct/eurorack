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
// Sub Oscillator: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/sub_oscillator_engine.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/polyblep.h"
#include "stmlib/dsp/units.h"

#include "plaits/dsp/oscillator/oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

void SubOscillator::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void SubOscillator::Reset() {
  phase_ = 0.0f;
  phase_sub_ = 0.0f;
  next_sample_square_ = 0.0f;
  next_sample_saw_ = 0.0f;
  next_sample_sub_ = 0.0f;
  high_square_ = false;
  high_saw_ = false;
  high_sub_ = false;
  previous_pw_square_ = 0.5f;
  previous_pw_saw_ = 0.25f;
  previous_pw_sub_ = 0.5f;
  frequency_ = 0.01f;
  pw_square_ = 0.5f;
  pw_saw_ = 0.25f;
  pw_sub_ = kSubOscillatorSubPwStock;
  colour_ = kSubOscillatorColourCentre;
  shape_ = 0.0f;
}

void SubOscillator::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  float frequency = NoteToFrequency(parameters.note);
  if (frequency >= kMaxFrequency) {
    frequency = kMaxFrequency;
  }

  float timbre = parameters.timbre;
  CONSTRAIN(timbre, 0.0f, 1.0f);
  const int32_t parameter = static_cast<int32_t>(
      timbre * kSubOscillatorParameterFullScale);

  const int32_t square_parameter = min(
      parameter, kSubOscillatorSquareParameterMax);
  const float target_pw_square = static_cast<float>(32768 - square_parameter) *
      kSubOscillatorParameterScale;

  const int32_t saw_parameter = max(
      parameter, kSubOscillatorSawParameterMin);
  const float target_pw_saw = static_cast<float>(saw_parameter) *
      kSubOscillatorParameterScale;

  float morph = parameters.morph;
  CONSTRAIN(morph, 0.0f, 1.0f);
  const int32_t colour = static_cast<int32_t>(
      morph * kSubOscillatorParameterFullScale);

  const float sub_octave =
      static_cast<float>(colour) < kSubOscillatorColourCentre
          ? kSubOscillatorLowOctave
          : kSubOscillatorHighOctave;
  const float sub_ratio = SemitonesToRatio(sub_octave);

  const float target_pw_sub = kSubOscillatorSubPwStock;

  float shape = parameters.harmonics;
  CONSTRAIN(shape, 0.0f, 1.0f);

  ParameterInterpolator fm(&frequency_, frequency, size);
  ParameterInterpolator pw_square_modulation(
      &pw_square_, target_pw_square, size);
  ParameterInterpolator pw_saw_modulation(&pw_saw_, target_pw_saw, size);
  ParameterInterpolator pw_sub_modulation(&pw_sub_, target_pw_sub, size);
  ParameterInterpolator colour_modulation(
      &colour_, static_cast<float>(colour), size);
  ParameterInterpolator shape_modulation(&shape_, shape, size);

  float next_square = next_sample_square_;
  float next_saw = next_sample_saw_;
  float next_sub = next_sample_sub_;

  for (size_t i = 0; i < size; ++i) {
    float this_square = next_square;
    float this_saw = next_saw;
    float this_sub = next_sub;
    next_square = 0.0f;
    next_saw = 0.0f;
    next_sub = 0.0f;

    float f = fm.Next();
    const float pw_square = pw_square_modulation.Next();
    const float pw_saw = pw_saw_modulation.Next();
    const float pw_sub = pw_sub_modulation.Next();
    const float shape = shape_modulation.Next();
    const float f_sub = f * sub_ratio;

    phase_ += f;
    while (true) {
      if (!high_square_ && phase_ >= pw_square) {
        float t = (phase_ - pw_square) / (previous_pw_square_ - pw_square + f);
        CONSTRAIN(t, 0.0f, 1.0f);
        this_square += ThisBlepSample(t);
        next_square += NextBlepSample(t);
        high_square_ = true;
        continue;
      }
      if (!high_saw_ && phase_ >= pw_saw) {
        float t = (phase_ - pw_saw) / (previous_pw_saw_ - pw_saw + f);
        CONSTRAIN(t, 0.0f, 1.0f);
        this_saw -= ThisBlepSample(t);
        next_saw -= NextBlepSample(t);
        high_saw_ = true;
        continue;
      }
      if (phase_ >= 1.0f) {
        phase_ -= 1.0f;
        const float t = phase_ / f;
        if (high_square_) {
          this_square -= ThisBlepSample(t);
          next_square -= NextBlepSample(t);
          high_square_ = false;
        }
        if (high_saw_) {
          this_saw -= ThisBlepSample(t);
          next_saw -= NextBlepSample(t);
          high_saw_ = false;
        }
        continue;
      }
      break;
    }
    next_square += phase_ < pw_square ? 0.0f : 1.0f;
    next_saw += phase_ < pw_saw
        ? (2.0f * phase_ - pw_saw)
        : (2.0f * phase_ - pw_saw - 1.0f);
    previous_pw_square_ = pw_square;
    previous_pw_saw_ = pw_saw;

    phase_sub_ += f_sub;
    while (true) {
      if (!high_sub_ && phase_sub_ >= pw_sub) {
        float t = (phase_sub_ - pw_sub) / (previous_pw_sub_ - pw_sub + f_sub);
        CONSTRAIN(t, 0.0f, 1.0f);
        this_sub += ThisBlepSample(t);
        next_sub += NextBlepSample(t);
        high_sub_ = true;
        continue;
      }
      if (phase_sub_ >= 1.0f) {
        phase_sub_ -= 1.0f;
        const float t = phase_sub_ / f_sub;
        if (high_sub_) {
          this_sub -= ThisBlepSample(t);
          next_sub -= NextBlepSample(t);
          high_sub_ = false;
        }
        continue;
      }
      break;
    }
    next_sub += phase_sub_ < pw_sub ? 0.0f : 1.0f;
    previous_pw_sub_ = pw_sub;

    // Remove each pulse's exact mean, as in Palette.
    const float square = 2.0f * this_square - 1.0f - (1.0f - 2.0f * pw_square);
    const float saw = this_saw;
    const float main = square + (saw - square) * shape;
    const float sub = 2.0f * this_sub - 1.0f - (1.0f - 2.0f * pw_sub);

    const float c = colour_modulation.Next();
    float gain = c < kSubOscillatorColourCentre
        ? (kSubOscillatorColourCentre - 1.0f - c) * 2.0f
        : (c - kSubOscillatorColourCentre) * 2.0f;
    if (gain < 0.0f) {
      gain = 0.0f;
    }

    out[i] = (main * (65535.0f - gain) + sub * gain) *
        kSubOscillatorParameterScale;
    aux[i] = sub;
  }

  next_sample_square_ = next_square;
  next_sample_saw_ = next_saw;
  next_sample_sub_ = next_sub;
}

}  // namespace plaits
