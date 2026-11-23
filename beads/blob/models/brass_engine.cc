// Copyright 1995-2023 Perry R. Cook and Gary P. Scavone.
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
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// Brass: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/brass_engine.h"

#include "beads/blob/models/palette_clear_buffer.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"


namespace plaits {

using namespace std;
using namespace stmlib;

void Brass::Init(BufferAllocator* allocator) {
  delay_line_ = allocator->Allocate<float>(kBrassDelaySize);
  Reset();
}

void Brass::Reset() {
  ClearBuffer(delay_line_, kBrassDelaySize * sizeof(delay_line_[0]));
  delay_write_ = 0;
  lip_x_ = 0.0f;
  lip_v_ = 0.0f;
  bell_lp_ = 0.0f;
  prev_arriving_ = 0.0f;
  junction_ = 0.0f;
  dc_x1_ = 0.0f;
  dc_y1_ = 0.0f;
  out_dc_x1_ = 0.0f;
  out_dc_y1_ = 0.0f;
  aux_dc_x1_ = 0.0f;
  aux_dc_y1_ = 0.0f;
}

void Brass::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    Reset();
  }

  float note = parameters.note;
  while (note < kBrassLowestNote) {
    note += 12.0f;
  }
  float frequency = NoteToFrequency(note) * kSampleRate;
  CONSTRAIN(frequency, 20.0f, 2000.0f);

  // The bore is tuned an octave below the played note, so the played note is
  const float bore_frequency = frequency * 0.5f;

  // Which partials this bore can offer. Computed from the uncompensated bore,
  int top = kBrassLastPartial;
  while (top > kBrassFirstPartial &&
      bore_frequency * float(top) > kBrassHighestPartial) {
    --top;
  }

  // HARMONICS is a continuous PARTIAL INDEX, not a lip frequency -- a linear
  const float span = float(top - kBrassFirstPartial);
  float position = float(kBrassFirstPartial) + parameters.harmonics * span;
  int partial = static_cast<int>(position);
  CONSTRAIN(partial, kBrassFirstPartial, top);
  const float within = position - static_cast<float>(partial);

  // Null the lip's pitch pull for the partial actually sounding.
  const float pull_cents = kBrassPullOffset +
      kBrassPullScale / static_cast<float>(partial);
  // SemitonesToRatio, not powf: the firmware is bare-metal and libm's powf
  const float pull_ratio = SemitonesToRatio(pull_cents * 0.01f);
  float length = kSampleRate / bore_frequency * pull_ratio;
  CONSTRAIN(length, 8.0f, float(kBrassDelaySize - 2));

  const float actual_bore = kSampleRate / length;
  const float lip_frequency = actual_bore * static_cast<float>(partial) *
      (kBrassZoneBase + kBrassZoneSpan * within);
  float lip_clamped = lip_frequency;
  CONSTRAIN(lip_clamped, 10.0f, 0.40f * kSampleRate);
  const float lip_w = 2.0f * float(M_PI) * lip_clamped / kSampleRate;

  // TIMBRE is breath pressure. Below the oscillation threshold the model is
  const float mouth = 0.18f + 0.50f * parameters.timbre;

  // MORPH is the bell. Its radiation corner tracks the played note rather than
  const float cutoff_ratio = 4.0f + 22.0f * parameters.morph;
  float damp = 2.0f * float(M_PI) * cutoff_ratio * frequency / kSampleRate;
  CONSTRAIN(damp, 0.02f, 0.90f);

  const float read_offset = length;

  // MACRO is the buzz: how far the lips swing into closure each cycle. It is
  const float rest_opening =
      kBrassRestOpening * (0.5f + 0.5f);

  // The steepening displacement is bounded by the bore length, not by a fixed
  const float steepening_limit = kBrassSteepeningLimit * read_offset;

  for (size_t i = 0; i < size; ++i) {
    float sample_read_offset = read_offset;
    float sample_lip_w = lip_w;
    float sample_damp = damp;
    float sample_steepening_limit = steepening_limit;
    // Nonlinear propagation: a high-pressure wave arrives early. Driven by the
    float steepening = kBrassSteepening * prev_arriving_;
    if (steepening > sample_steepening_limit) {
      steepening = sample_steepening_limit;
    } else if (steepening < -sample_steepening_limit) {
      steepening = -sample_steepening_limit;
    }

    float read_position =
        static_cast<float>(delay_write_) -
        (sample_read_offset - steepening);
    while (read_position < 0.0f) {
      read_position += static_cast<float>(kBrassDelaySize);
    }
    const int integral = static_cast<int>(read_position);
    const float fractional = read_position - static_cast<float>(integral);
    const int a = integral % kBrassDelaySize;
    const int b = (integral + 1) % kBrassDelaySize;
    const float arriving = delay_line_[a] +
        (delay_line_[b] - delay_line_[a]) * fractional;
    prev_arriving_ = arriving;

    // The bell: what it does not reflect, it radiates.
    bell_lp_ += sample_damp * (arriving - bell_lp_);
    const float p_minus = kBrassReflection * bell_lp_;

    const float breath = mouth *
        (1.0f + kBrassNoiseGain * (2.0f * Random::GetFloat() - 1.0f));

    // The valve sees the TOTAL junction pressure, one sample late. Feeding it
    const float delta = breath - junction_;

    // Outward-striking lip: rising pressure opens it.
    const float target = delta / kBrassLipStiffness;
    lip_v_ += sample_lip_w * sample_lip_w * (target - lip_x_)
        - 2.0f * kBrassLipZeta * sample_lip_w * lip_v_;
    lip_x_ += lip_v_;

    float opening = rest_opening + lip_x_;
    if (opening < 0.0f) {
      // Lips closed: no flow, and they stop rather than keep travelling.
      opening = 0.0f;
      if (lip_v_ < 0.0f) {
        lip_v_ = 0.0f;
      }
    } else if (opening > kBrassMaxOpening) {
      opening = kBrassMaxOpening;
    }

    // Bernoulli flow through the opening.
    const float flow = opening *
        (delta >= 0.0f ? Sqrt(delta) : -Sqrt(-delta));

    const float p_plus = p_minus + kBrassBoreImpedance * flow;
    junction_ = p_plus + p_minus;

    // The loop is non-inverting so it has a DC mode; without this the model
    const float blocked = p_plus - dc_x1_ + 0.9995f * dc_y1_;
    dc_x1_ = p_plus;
    dc_y1_ = blocked;

    delay_line_[delay_write_] = blocked;
    delay_write_ = (delay_write_ + 1) % kBrassDelaySize;

    // Blowing produces a steady flow as well as an oscillation, so both taps
    const float out_ac = junction_ - out_dc_x1_ + 0.9995f * out_dc_y1_;
    out_dc_x1_ = junction_;
    out_dc_y1_ = out_ac;

    const float valve_flow = kBrassBoreImpedance * flow;
    const float aux_ac = valve_flow - aux_dc_x1_ + 0.9995f * aux_dc_y1_;
    aux_dc_x1_ = valve_flow;
    aux_dc_y1_ = aux_ac;

    // OUT is the pressure inside the mouthpiece: flat to 0.7 dB from note 40
    out[i] = SoftClip(out_ac * 1.6f);
    // AUX is the valve flow -- the same note heard as the buzz that drives it.
    aux[i] = SoftClip(aux_ac * 6.0f);
  }
}

}  // namespace plaits
