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
// Gendy: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/gendy_engine.h"

#include <algorithm>

#include "stmlib/utils/random.h"

namespace plaits {

using namespace std;
using namespace stmlib;

void Gendy::Init(BufferAllocator* allocator) {
  Reset();
}

void Gendy::Reset() {
  phase_ = 0.0f;
  segment_ = 0;
  num_breakpoints_ = 0;
}

float Gendy::Walk(
    float value,
    float amount,
    float minimum,
    float maximum) {
  value += (2.0f * Random::GetFloat() - 1.0f) * amount;
  if (value > maximum) {
    value = maximum - (value - maximum);
  } else if (value < minimum) {
    value = minimum + (minimum - value);
  }
  CONSTRAIN(value, minimum, maximum);
  return value;
}

void Gendy::Randomize(int num_breakpoints) {
  num_breakpoints_ = num_breakpoints;
  for (int i = 0; i < num_breakpoints_; ++i) {
    amplitude_[i] = 2.0f * Random::GetFloat() - 1.0f;
    duration_[i] = 0.5f + Random::GetFloat();
  }
  UpdateBoundaries();
  phase_ = 0.0f;
  segment_ = 0;
}

void Gendy::UpdateBoundaries() {
  float total = 0.0f;
  for (int i = 0; i < num_breakpoints_; ++i) {
    total += duration_[i];
  }
  float cumulative = 0.0f;
  for (int i = 0; i < num_breakpoints_; ++i) {
    cumulative += duration_[i] / total;
    boundary_[i] = cumulative;
  }
  boundary_[num_breakpoints_ - 1] = 1.0f;
}

void Gendy::Mutate(float amplitude_step, float duration_step) {
  float mean = 0.0f;
  for (int i = 0; i < num_breakpoints_; ++i) {
    amplitude_[i] = Walk(amplitude_[i], amplitude_step, -1.0f, 1.0f);
    duration_[i] = Walk(duration_[i], duration_step, 0.2f, 2.5f);
    mean += amplitude_[i];
  }
  mean /= static_cast<float>(num_breakpoints_);
  for (int i = 0; i < num_breakpoints_; ++i) {
    amplitude_[i] -= mean;
    CONSTRAIN(amplitude_[i], -1.0f, 1.0f);
  }
  UpdateBoundaries();
}

void Gendy::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  // Three to nine breakpoints retain a broad stochastic shape range.
  const int num_breakpoints = 3 + static_cast<int>(
      parameters.harmonics * 6.999f);
  if (num_breakpoints != num_breakpoints_ ||
      (parameters.trigger & TRIGGER_RISING_EDGE)) {
    Randomize(num_breakpoints);
  }

  const float frequency = min(0.24f, NoteToFrequency(parameters.note));
  const float complexity_compensation = 1.0f - 0.035f *
      static_cast<float>(num_breakpoints - 3);
  const float amplitude_step = (0.005f +
      0.3f * parameters.timbre * parameters.timbre) *
      complexity_compensation;
  const float duration_step = (0.005f +
      0.75f * parameters.morph * parameters.morph) *
      complexity_compensation;

  for (size_t i = 0; i < size; ++i) {
    float f = frequency;
    bool cycle;
    {
      phase_ += f;
      cycle = phase_ >= 1.0f;
      if (cycle) {
        phase_ -= 1.0f;
      }
    }
    if (cycle) {
      Mutate(amplitude_step, duration_step);
      segment_ = 0;
    }
    while (segment_ < num_breakpoints_ - 1 &&
        phase_ >= boundary_[segment_]) {
      ++segment_;
    }

    const int next = segment_ + 1 == num_breakpoints_ ? 0 : segment_ + 1;
    const float start = segment_ == 0 ? 0.0f : boundary_[segment_ - 1];
    const float width = boundary_[segment_] - start;
    float t = (phase_ - start) / width;
    CONSTRAIN(t, 0.0f, 1.0f);

    const float stepped = amplitude_[segment_];
    const float linear = stepped + (amplitude_[next] - stepped) * t;
    const float smooth_t = t * t * (3.0f - 2.0f * t);
    const float smooth = stepped + (amplitude_[next] - stepped) * smooth_t;
    float sample;
    if (0.5f < 0.5f) {
      sample = stepped + (linear - stepped) * 0.5f * 2.0f;
    } else {
      sample = linear + (smooth - linear) * (0.5f * 2.0f - 1.0f);
    }
    out[i] = sample * 0.8f;
    aux[i] = stepped * 0.65f;

  }
}

}  // namespace plaits
