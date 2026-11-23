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

// Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/toy_engine.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"

#include "plaits/dsp/downsampler/4x_downsampler.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

inline uint8_t MangleRamp(uint8_t ramp, uint8_t x, int bias) {
  const int masked = (ramp ^ (x << 1)) & (~static_cast<int>(x));
  return static_cast<uint8_t>(masked + bias);
}

}  // namespace

void Toy::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void Toy::Reset() {
  phase_ = 0.0f;
  ramp_increment_ = 0.0f;
  decimation_counter_ = 0.0f;
  held_sample_ = 128;
  decimation_counter_r_ = 0.0f;
  held_sample_r_ = 128;
  downsampler_state_ = 0.0f;
  downsampler_state_r_ = 0.0f;
  dc_input_ = 0.0f;
  dc_output_ = 0.0f;
  dc_input_aux_ = 0.0f;
  dc_output_aux_ = 0.0f;
}

void Toy::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    phase_ = 0.0f;
    decimation_counter_ = 0.0f;
    decimation_counter_r_ = 0.0f;
  }

  const float target_increment = NoteToFrequency(parameters.note) * 0.25f;

  const uint8_t x = static_cast<uint8_t>(parameters.harmonics * 127.0f);

  const float fold = 0.5f;
  const int bias = static_cast<int>(static_cast<float>(x) * fold);

  float free_count = kToyMaxDecimation - \
      (kToyMaxDecimation - 1.0f) * parameters.timbre;

  const float note_frequency = max(1e-6f, NoteToFrequency(parameters.note));
  const float tracked_count = free_count * \
      NoteToFrequency(60.0f) / note_frequency;
  float count = free_count + \
      (tracked_count - free_count) * parameters.morph;
  CONSTRAIN(count, 1.0f, 4096.0f);

  ParameterInterpolator increment_modulation(
      &ramp_increment_, target_increment, size);

  Downsampler downsampler(&downsampler_state_);

  for (size_t i = 0; i < size; ++i) {
    float increment = increment_modulation.Next();

    float raw = 0.0f;

    for (int t = 0; t < 4; ++t) {
      phase_ += increment;
      if (phase_ >= 1.0f) {
        phase_ -= 1.0f;
      } else if (phase_ < 0.0f) {
        phase_ += 1.0f;
      }
      const uint8_t ramp = static_cast<uint8_t>(phase_ * 256.0f);

      decimation_counter_ += 1.0f;
      if (decimation_counter_ >= count) {
        held_sample_ = MangleRamp(ramp, x, bias);
        decimation_counter_ = 0.0f;
      }
      const float sample = \
          (static_cast<float>(held_sample_) - 128.0f) * (1.0f / 128.0f);
      downsampler.Accumulate(t, sample);
      raw = sample;

    }

    const float filtered = downsampler.Read();
    const float dc_out = filtered - dc_input_ + 0.9975f * dc_output_;
    dc_input_ = filtered;
    dc_output_ = dc_out;
    out[i] = dc_out;

    const float dc_aux = raw - dc_input_aux_ + 0.9975f * dc_output_aux_;
    dc_input_aux_ = raw;
    dc_output_aux_ = dc_aux;
    aux[i] = dc_aux;

  }
}

}  // namespace plaits
