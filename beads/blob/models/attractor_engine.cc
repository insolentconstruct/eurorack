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
// Attractor: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/attractor_engine.h"

#include <algorithm>
#include <cmath>

#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

const float kInverseTwoPi = 0.159154943f;

inline float StateSine(float state) {
  return Sine(8.0f + state * kInverseTwoPi);
}

inline float ShapeCoordinate(float value) {
  return value / (1.0f + fabsf(value));
}

inline float LimitAudio(float sample) {
  CONSTRAIN(sample, -1.0f, 1.0f);
  return sample;
}

}  // namespace

void Attractor::Init(BufferAllocator* allocator) {
  Reset();
}

void Attractor::Reset() {
  state_[0] = 0.0f;
  state_[1] = 0.0f;
  state_[2] = 0.0f;
  dc_[0] = 0.0f;
  dc_[1] = 0.0f;
  dc_[2] = 0.0f;
  reset_pending_ = true;
}

void Attractor::Seed(float accent) {
  state_[0] = 0.17f + 0.21f * accent;
  state_[1] = -0.11f;
  state_[2] = 0.07f - 0.13f * accent;
}

void Attractor::Step(
    float rate_x,
    float rate_y,
    float rate_z,
    float damping,
    float argument_scale) {
  const float x = state_[0];
  const float y = state_[1];
  const float z = state_[2];
  state_[0] += rate_x * (StateSine(y * argument_scale) - damping * x);
  state_[1] += rate_y * (StateSine(z * argument_scale) - damping * y);
  state_[2] += rate_z * (StateSine(x * argument_scale) - damping * z);

  CONSTRAIN(state_[0], -7.0f, 7.0f);
  CONSTRAIN(state_[1], -7.0f, 7.0f);
  CONSTRAIN(state_[2], -7.0f, 7.0f);
}

void Attractor::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  if (reset_pending_ || (parameters.trigger & TRIGGER_RISING_EDGE)) {
    Seed(parameters.accent);
    reset_pending_ = false;
  }

  const float base_frequency = NoteToFrequency(parameters.note);
  const float skew = (0.5f - 0.5f) * 0.78f;

  const float argument_scale = 0.68f +
      0.92f * parameters.harmonics * parameters.harmonics;
  const float chaos_floor = 0.19f - 0.10f * parameters.harmonics;
  const float damping = argument_scale *
      (0.315f + (chaos_floor - 0.315f) * parameters.timbre);

  for (size_t i = 0; i < size; ++i) {
    float frequency = base_frequency;
    const float base_rate = min(0.62f, frequency * 6.283185307f);
    const float rate_x = base_rate * (1.0f + skew);
    const float rate_y = base_rate;
    const float rate_z = base_rate * (1.0f - skew);
    Step(rate_x, rate_y, rate_z, damping, argument_scale);

    float coordinate[3];
    for (int j = 0; j < 3; ++j) {
      const float shaped = ShapeCoordinate(state_[j]);
      dc_[j] += 0.00022f * (shaped - dc_[j]);
      coordinate[j] = shaped - dc_[j];
    }

    const float selector = parameters.morph * 2.0f;
    float main_coordinate;
    float aux_coordinate;
    if (selector < 1.0f) {
      main_coordinate = coordinate[0] +
          (coordinate[1] - coordinate[0]) * selector;
      aux_coordinate = coordinate[2] +
          (coordinate[0] - coordinate[2]) * selector;
    } else {
      const float fraction = selector - 1.0f;
      main_coordinate = coordinate[1] +
          (coordinate[2] - coordinate[1]) * fraction;
      aux_coordinate = coordinate[0] +
          (coordinate[1] - coordinate[0]) * fraction;
    }
    out[i] = LimitAudio(0.95f * SoftClip(main_coordinate * 3.0f));
    aux[i] = LimitAudio(0.95f * SoftClip(aux_coordinate * 3.0f));
  }
}

}  // namespace plaits
