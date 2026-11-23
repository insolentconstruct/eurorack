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
// Tapfield: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/tapfield_engine.h"

#include <algorithm>

namespace plaits {

using namespace std;
using namespace stmlib;

void Tapfield::Init(BufferAllocator* allocator) {
  Reset();
}

void Tapfield::Reset() {
  state_ = 1u;
  tap_mask_ = 1u;
  width_mask_ = 0x1ffu;
  clock_counter_ = 0u;
  register_length_ = -1;
  tap_family_ = -1;
  clock_phase_ = 0.0f;
  corruption_phase_ = 0.0f;
  target_ = 0.0f;
  value_ = 0.0f;
  aux_target_ = -1.0f;
  ConfigureTopology(0.0f);
  Seed();
}

void Tapfield::ConfigureTopology(float harmonics) {
  int selection = static_cast<int>(harmonics * 63.999f);
  selection = max(0, min(63, selection));
  const int length = 9 + (selection & 15);
  const int family = selection >> 4;
  if (length == register_length_ && family == tap_family_) {
    return;
  }

  register_length_ = length;
  tap_family_ = family;
  width_mask_ = (1u << register_length_) - 1u;

  const int span = register_length_ - 2;
  const int tap_a = 1 + (register_length_ + 3 * tap_family_) % span;
  const int tap_b = 1 + (register_length_ / 2 +
      5 * tap_family_ + 1) % span;
  const int tap_c = 1 + (register_length_ / 3 +
      7 * tap_family_ + 3) % span;
  tap_mask_ = (1u << (register_length_ - 1)) |
      (1u << tap_a) | (1u << tap_b) | (1u << tap_c);

  state_ &= width_mask_;
  if (!state_) {
    Seed();
  }
}

void Tapfield::Seed() {
  uint32_t seed = 0x6d2b79f5u ^
      static_cast<uint32_t>(register_length_ * 0x45d9f3bu) ^
      static_cast<uint32_t>(tap_family_ * 0x119de1f3u);
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  state_ = seed & width_mask_;
  if (!state_) {
    state_ = (1u << (register_length_ - 1)) | 1u;
  }
  clock_counter_ = 0u;
  corruption_phase_ = 0.0f;
  aux_target_ = (state_ & 1u) ? 1.0f : -1.0f;
}

void Tapfield::Clock(float corruption) {
  const uint32_t carry = state_ & 1u;
  state_ >>= 1;
  if (carry) {
    state_ ^= tap_mask_;
  }
  state_ &= width_mask_;
  ++clock_counter_;
  aux_target_ = carry ? 1.0f : -1.0f;

  corruption_phase_ += 0.45f * corruption * corruption;
  if (corruption_phase_ >= 1.0f) {
    corruption_phase_ -= 1.0f;
    const uint32_t folded = state_ ^ (state_ >> 7) ^
        (clock_counter_ * 0x9e37u);
    const int bit = static_cast<int>(
        folded % static_cast<uint32_t>(register_length_));
    state_ ^= 1u << bit;
  }
  if (!state_) {
    state_ = (1u << (register_length_ - 1)) | 1u;
  }
}

float Tapfield::Decode(float timbre) const {
  const uint32_t gray = state_ ^ (state_ >> 1);
  const float gray_amount = timbre;
  const float weight_amount = timbre * timbre;
  float value = 0.0f;
  float weight_sum = 0.0f;
  for (int i = 0; i < 8; ++i) {
    const int bit = i * (register_length_ - 1) / 7;
    const float direct_value = (state_ & (1u << bit)) ? 1.0f : -1.0f;
    const float gray_value = (gray & (1u << bit)) ? 1.0f : -1.0f;
    const float bit_value = direct_value +
        (gray_value - direct_value) * gray_amount;
    const float binary_weight = static_cast<float>(1u << i);
    const float weight = 1.0f +
        (binary_weight - 1.0f) * weight_amount;
    value += bit_value * weight;
    weight_sum += weight;
  }
  return value / weight_sum;
}

void Tapfield::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  ConfigureTopology(parameters.harmonics);
  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    Seed();
    clock_phase_ = 0.0f;
    target_ = Decode(parameters.timbre);
  }

  const float base_frequency = NoteToFrequency(parameters.note);
  const float one_minus_slew = 1.0f - 0.5f;
  const float slew = 0.0025f + 0.9975f * one_minus_slew *
      one_minus_slew * one_minus_slew;

  for (size_t i = 0; i < size; ++i) {
    float frequency = base_frequency;
    const float clock_frequency = min(0.45f, 4.0f * frequency);
    clock_phase_ += clock_frequency;
    if (clock_phase_ >= 1.0f) {
      clock_phase_ -= 1.0f;
      Clock(parameters.morph);
      target_ = Decode(parameters.timbre);
    }
    value_ += slew * (target_ - value_);
    out[i] = 0.9f * value_;
    aux[i] = 0.72f * aux_target_;
  }
}

}  // namespace plaits
