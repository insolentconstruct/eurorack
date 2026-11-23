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
// Rulefield: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/rulefield_engine.h"

#include <algorithm>

namespace plaits {
using namespace std;
using namespace stmlib;

uint32_t Rulefield::RotateLeft(uint32_t value) {
  return (value << 1) | (value >> 31);
}

uint32_t Rulefield::RotateRight(uint32_t value) {
  return (value >> 1) | (value << 31);
}

float Rulefield::Density(uint32_t value) {
  value -= (value >> 1) & 0x55555555u;
  value = (value & 0x33333333u) + ((value >> 2) & 0x33333333u);
  value = (value + (value >> 4)) & 0x0f0f0f0fu;
  value += value >> 8;
  value += value >> 16;
  return static_cast<float>(value & 0x3fu) * (1.0f / 32.0f);
}

void Rulefield::Init(BufferAllocator* allocator) {
  Reset();
}

void Rulefield::Reset() {
  rule_ = 30;
  generation_ = 0u;
  phase_ = 0.0f;
  evolution_phase_ = 0.0f;
  Seed();
}

void Rulefield::Seed() {
  uint32_t seed = 0xa3619d27u ^
      (static_cast<uint32_t>(rule_) * 0x45d9f3bu);
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  row_ = seed;
  if (!row_ || row_ == 0xffffffffu) {
    row_ = 0x80000001u;
  }
  previous_row_ = RotateRight(row_);
  generation_ = 0u;
  UpdateReadouts();
}

void Rulefield::UpdateReadouts() {
  edge_row_ = row_ ^ RotateRight(row_);
  activity_row_ = row_ ^ previous_row_;
  row_density_ = Density(row_);
  edge_density_ = Density(edge_row_);
  activity_density_ = Density(activity_row_);
}

void Rulefield::Evolve() {
  const uint32_t left = RotateLeft(row_);
  const uint32_t center = row_;
  const uint32_t right = RotateRight(row_);
  uint32_t next = 0u;
  for (int neighborhood = 0; neighborhood < 8; ++neighborhood) {
    if (!(rule_ & (1u << neighborhood))) {
      continue;
    }
    const uint32_t left_match = neighborhood & 4 ? left : ~left;
    const uint32_t center_match = neighborhood & 2 ? center : ~center;
    const uint32_t right_match = neighborhood & 1 ? right : ~right;
    next |= left_match & center_match & right_match;
  }

  previous_row_ = row_;
  row_ = next;
  ++generation_;

  // Keep absorbing rows audible with the original deterministic cell walk.
  const int injection = static_cast<int>((generation_ * 13u + rule_) & 31u);
  if (!row_) {
    row_ = 1u << injection;
  } else if (row_ == 0xffffffffu) {
    row_ &= ~(1u << injection);
  }
  UpdateReadouts();
}

float Rulefield::ReadWave(
    uint32_t row,
    float density,
    float phase,
    float shape) const {
  const float address = phase * 32.0f;
  const int index = static_cast<int>(address) & 31;
  const int next_index = (index + 1) & 31;
  const float fraction = address - static_cast<int>(address);
  const float a = (row & (1u << index)) ? 1.0f - density : -density;
  const float b = (row & (1u << next_index)) ? 1.0f - density : -density;
  const float linear = a + (b - a) * fraction;
  const float smooth_fraction = fraction * fraction *
      (3.0f - 2.0f * fraction);
  const float smooth = a + (b - a) * smooth_fraction;
  return shape < 0.5f
      ? a + (linear - a) * (2.0f * shape)
      : linear + (smooth - linear) * (2.0f * shape - 1.0f);
}

void Rulefield::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  const uint8_t rule = static_cast<uint8_t>(
      1 + static_cast<int>(253.999f * parameters.harmonics));
  if (rule != rule_) {
    rule_ = rule;
  }
  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    Seed();
    phase_ = 0.0f;
    evolution_phase_ = 0.0f;
  }

  const float base_frequency = min(0.24f, NoteToFrequency(parameters.note));
  const float evolution_rate = 0.125f + 7.875f *
      parameters.morph * parameters.morph;

  for (size_t i = 0; i < size; ++i) {
    float frequency = base_frequency;
    bool cycle;
    {
      phase_ += frequency;
      cycle = phase_ >= 1.0f;
      if (cycle) {
        phase_ -= 1.0f;
      }
    }
    if (cycle) {
      evolution_phase_ += evolution_rate;
      int iterations = 0;
      while (evolution_phase_ >= 1.0f && iterations < 8) {
        evolution_phase_ -= 1.0f;
        Evolve();
        ++iterations;
      }
    }

    const float cells = ReadWave(
        row_, row_density_, phase_, 0.5f);
    const float edges = ReadWave(
        edge_row_, edge_density_, phase_, 0.5f);
    out[i] = 0.9f * (cells + (edges - cells) * parameters.timbre);
    const float activity = ReadWave(
        activity_row_, activity_density_, phase_, 0.5f);
    aux[i] = 0.9f * activity;
  }
}

}  // namespace plaits
