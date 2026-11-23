// Copyright 2012 Olivier Gillet.
// Copyright 2015 Tim Churches.
// Copyright 2026 Lyle Mills.
// SPDX-License-Identifier: MIT
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
// Palette port: mono, no FM input, fourth macro fixed at 0.5.

#include "beads/blob/models/bytebeat_engine.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

inline uint32_t EvaluateBytebeat(int formula, uint32_t t, uint32_t p0, uint32_t p1) {
  switch (formula) {
    case 0:
      return ((t * 3) & (t >> 10))
          | ((t * p0) & (t >> 10))
          | ((t * 10) & ((t >> 8) * p1) & 128);
    case 1:
      return ((t * p0) & (t >> 4))
          | ((t * 5) & (t >> 7))
          | ((t * p1) & (t >> 10));
    case 2:
      return ((t >> p0) & t) * (t >> p1);
    default:
      return ((((((t >> p0) | t) | (t >> p0)) * 10) & ((5 * t) | (t >> 10)))
          | (t ^ (t % p1)));
  }
}

inline float MaskedSample(uint32_t value, int width) {
  const uint32_t span = 1u << width;
  const uint32_t half = span >> 1;
  const uint32_t masked = value & (span - 1u);
  const int32_t centred = masked >= half
      ? static_cast<int32_t>(masked) - static_cast<int32_t>(span)
      : static_cast<int32_t>(masked);
  return static_cast<float>(centred) / static_cast<float>(half);
}

}  // namespace

void Bytebeat::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void Bytebeat::Reset() {
  tick_phase_ = 0.0f;
  aux_tick_phase_ = 0.0f;
  t_ = kBytebeatRestartTick;
  aux_t_ = kBytebeatRestartTick;
  dc_in_ = 0.0f;
  dc_out_ = 0.0f;
  dc_aux_in_ = 0.0f;
  dc_aux_out_ = 0.0f;
}

void Bytebeat::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    tick_phase_ = 0.0f;
    aux_tick_phase_ = 0.0f;
    t_ = kBytebeatRestartTick;
    aux_t_ = kBytebeatRestartTick;
  }

  int formula = static_cast<int>(parameters.morph * kBytebeatNumFormulas);
  CONSTRAIN(formula, 0, kBytebeatNumFormulas - 1);

  uint32_t p0;
  uint32_t p1;
  if (formula == 0) {
    p0 = static_cast<uint32_t>(parameters.harmonics * 63.0f);
    p1 = static_cast<uint32_t>(parameters.timbre * 15.0f);
  } else if (formula == 3) {
    p0 = static_cast<uint32_t>(parameters.harmonics * 15.0f);
    p1 = 1u + static_cast<uint32_t>(parameters.timbre * 126.0f);
  } else {
    p0 = static_cast<uint32_t>(parameters.harmonics * 15.0f);
    p1 = static_cast<uint32_t>(parameters.timbre * 15.0f);
  }

  int width = static_cast<int>(8.0f);
  CONSTRAIN(width, 2, 12);

  const float f0 = NoteToFrequency(parameters.note);
  const float tick_increment = f0 * kBytebeatTicksPerCycle;
  const float aux_tick_increment = tick_increment * kBytebeatAuxRatio;

  for (size_t i = 0; i < size; ++i) {
    float main_increment = tick_increment;
    float secondary_increment = aux_tick_increment;
    {
    tick_phase_ += main_increment;
    while (tick_phase_ >= 1.0f) {
      tick_phase_ -= 1.0f;
      ++t_;
    }
    aux_tick_phase_ += secondary_increment;
    while (aux_tick_phase_ >= 1.0f) {
      aux_tick_phase_ -= 1.0f;
      ++aux_t_;
    }
    }

    const float sample = MaskedSample(
        EvaluateBytebeat(formula, t_, p0, p1), width);
    const float aux_sample = MaskedSample(
        EvaluateBytebeat(formula, aux_t_, p0, p1), width);

    dc_out_ = sample - dc_in_ + kBytebeatDcPole * dc_out_;
    dc_in_ = sample;
    out[i] = dc_out_;

    dc_aux_out_ = aux_sample - dc_aux_in_ + kBytebeatDcPole * dc_aux_out_;
    dc_aux_in_ = aux_sample;
    aux[i] = dc_aux_out_;
  }
}

}  // namespace plaits
