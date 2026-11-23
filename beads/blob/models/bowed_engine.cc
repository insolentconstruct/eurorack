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

#include "beads/blob/models/bowed_engine.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

inline int8_t QuantizeToInt8(float value) {
  const float scaled = value * 128.0f;
  int quantized = static_cast<int>(scaled);
  if (scaled < static_cast<float>(quantized)) {
    --quantized;
  }
  CONSTRAIN(quantized, -128, 127);
  return static_cast<int8_t>(quantized);
}

inline int32_t MixInt8(int32_t a, int32_t b, uint32_t balance) {
  const int32_t b_int = static_cast<int32_t>(balance);
  return (a * (65535 - b_int) + b * b_int) >> 16;
}

inline float BowingFriction(float d) {
  const float g = d + 0.75f;
  const float g2 = g * g;
  float friction = 1.0f / (g2 * g2);
  if (friction > 1.0f) {
    friction = 1.0f;
  }
  const int32_t quantized = static_cast<int32_t>(friction * 32768.0f);
  return static_cast<float>(quantized) * (1.0f / 32768.0f);
}

inline float BowingEnvelope(float index) {
  if (index >= kBowedEnvelopeHold) {
    return kBowedEnvelopeSustain;
  }
  if (index <= kBowedEnvelopeRise) {
    return kBowedEnvelopePeak * (index / kBowedEnvelopeRise);
  }
  const float t = (index - kBowedEnvelopeRise) / \
      (kBowedEnvelopeHold - kBowedEnvelopeRise);
  return kBowedEnvelopePeak + \
      (kBowedEnvelopeSustain - kBowedEnvelopePeak) * t;
}

inline void ResolveBowedDelays(
    float frequency,
    float bow_position,
    uint32_t* bridge_integral,
    uint32_t* neck_integral,
    uint32_t* bridge_balance,
    uint32_t* neck_balance) {
  float delay = 1.0f / max(1e-7f, frequency) - 2.0f;
  float bridge_delay = delay * bow_position;
  int guard = 0;
  while (guard < 16 &&
         ((delay - bridge_delay) > static_cast<float>(kBowedNeckLength - 1) ||
          bridge_delay > static_cast<float>(kBowedBridgeLength - 1))) {
    delay *= 0.5f;
    bridge_delay *= 0.5f;
    ++guard;
  }
  CONSTRAIN(bridge_delay, 1.0f,
            static_cast<float>(kBowedBridgeLength - 4));
  float neck_delay = delay - bridge_delay;
  CONSTRAIN(neck_delay, 4.0f,
            static_cast<float>(kBowedNeckLength - 4));

  *bridge_integral = static_cast<uint32_t>(bridge_delay);
  *neck_integral = static_cast<uint32_t>(neck_delay);
  *bridge_balance = static_cast<uint32_t>(
      (bridge_delay - static_cast<float>(*bridge_integral)) * 65536.0f);
  *neck_balance = static_cast<uint32_t>(
      (neck_delay - static_cast<float>(*neck_integral)) * 65536.0f);
  if (*bridge_balance > 65535) {
    *bridge_balance = 65535;
  }
  if (*neck_balance > 65535) {
    *neck_balance = 65535;
  }
}

}  // namespace

void Bowed::Init(BufferAllocator* allocator) {
  bridge_line_ = allocator->Allocate<int8_t>(kBowedBridgeLength);
  neck_line_ = allocator->Allocate<int8_t>(kBowedNeckLength);
  Reset();
}

void Bowed::Reset() {
  if (bridge_line_) {
    std::fill(&bridge_line_[0], &bridge_line_[kBowedBridgeLength], 0);
  }
  if (neck_line_) {
    std::fill(&neck_line_[0], &neck_line_[kBowedNeckLength], 0);
  }
  delay_pointer_ = 0;
  excitation_ = 0.0f;
  bridge_lp_ = 0.0f;
  body_y0_ = 0.0f;
  body_y1_ = 0.0f;
  body_aux_y0_ = 0.0f;
  body_aux_y1_ = 0.0f;
  tilt_state_ = 0.0f;
  tilt_state_aux_ = 0.0f;
  exciter_dc_in_ = 0.0f;
  exciter_dc_out_ = 0.0f;
}

void Bowed::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (!bridge_line_ || !neck_line_) {
    for (size_t i = 0; i < size; ++i) {
      out[i] = 0.0f;
      aux[i] = 0.0f;
    }
    return;
  }

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    Reset();
  }

  const float frequency = max(1e-7f, NoteToFrequency(parameters.note));

  const int bow_steps = 6 + (static_cast<int>(
      parameters.harmonics * 32767.0f) >> 9);
  const float bow_position = static_cast<float>(bow_steps) * (1.0f / 256.0f);
  uint32_t bridge_integral;
  uint32_t neck_integral;
  uint32_t bridge_balance;
  uint32_t neck_balance;
  ResolveBowedDelays(
      frequency, bow_position,
      &bridge_integral, &neck_integral,
      &bridge_balance, &neck_balance);

  const int pressure_steps = 172 - (static_cast<int>(
      parameters.timbre * 32767.0f) >> 8);
  const float pressure = static_cast<float>(pressure_steps) * (1.0f / 32.0f);

  const float nut_gain = 0.55f + 0.45f * parameters.morph;

  const float body_offset = 0.0f;
  const float theta = kBowedStockTheta * SemitonesToRatio(body_offset);
  const float body_a1 = 2.0f * kBowedBodyRadius * \
      SineNoWrap(theta * (1.0f / 6.2831853f) + 0.25f);
  const float body_a2 = -kBowedBodyRadius * kBowedBodyRadius;

  for (size_t i = 0; i < size; ++i) {

    const uint32_t bridge_read = \
        (delay_pointer_ + 2 * kBowedBridgeLength - bridge_integral);
    const uint32_t neck_read = \
        (delay_pointer_ + 2 * kBowedNeckLength - neck_integral);

    const int32_t bridge_a = \
        bridge_line_[bridge_read & (kBowedBridgeLength - 1)];
    const int32_t bridge_b = \
        bridge_line_[(bridge_read - 1) & (kBowedBridgeLength - 1)];
    const int32_t neck_a = neck_line_[neck_read & (kBowedNeckLength - 1)];
    const int32_t neck_b = neck_line_[(neck_read - 1) & (kBowedNeckLength - 1)];

    const float bridge_value = static_cast<float>(
        MixInt8(bridge_a, bridge_b, bridge_balance)) * (1.0f / 128.0f);
    const float nut_value = static_cast<float>(
        MixInt8(neck_a, neck_b, neck_balance)) * (1.0f / 128.0f);

    bridge_lp_ = kBowedBridgeLpGain * bridge_value + \
        kBowedBridgeLpPole * bridge_lp_;
    const float bridge_reflection = -bridge_lp_;
    const float nut_reflection = -nut_gain * nut_value;
    const float string_velocity = bridge_reflection + nut_reflection;

    const float bow_velocity = BowingEnvelope(excitation_);
    const float velocity_delta = bow_velocity - string_velocity;

    float friction_argument = velocity_delta * pressure;
    if (friction_argument < 0.0f) {
      friction_argument = -friction_argument;
    }
    int friction_index = static_cast<int>(friction_argument * 64.0f);
    if (friction_index > 255) {
      friction_index = 255;
    }
    const float new_velocity = BowingFriction(
        static_cast<float>(friction_index) * (1.0f / 64.0f)) * velocity_delta;

    neck_line_[delay_pointer_ & (kBowedNeckLength - 1)] = \
        QuantizeToInt8(bridge_reflection + new_velocity);
    bridge_line_[delay_pointer_ & (kBowedBridgeLength - 1)] = \
        QuantizeToInt8(nut_reflection + new_velocity);
    ++delay_pointer_;

    const float body = kBowedBodyGain * bridge_value + \
        body_a1 * body_y0_ + body_a2 * body_y1_;
    const float bridge_out = body - body_y1_;
    body_y1_ = body_y0_;
    body_y0_ = body;

    tilt_state_ += (1.0f - kBowedTilt) * (bridge_out - tilt_state_);

    out[i] = SoftClip(1.6f * tilt_state_);

    exciter_dc_out_ = new_velocity - exciter_dc_in_ + \
        kBowedExciterDcPole * exciter_dc_out_;
    exciter_dc_in_ = new_velocity;
    aux[i] = SoftClip(kBowedExciterGain * exciter_dc_out_);

    excitation_ += 0.5f;
    if (excitation_ > kBowedEnvelopeHold) {
      excitation_ = kBowedEnvelopeHold;
    }
  }
}

}  // namespace plaits
