// Copyright 2026 Combust.
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

#include "beads/blob/models/zxphase48k_engine.h"

namespace plaits {

using namespace stmlib;

namespace {

const float kTwo32 = 4294967296.0f;
const float kTwo30 = 1073741824.0f;  // one internal (4x) step per sample

const uint32_t kGhostIncrement = 594394112u;  // 26500/191489 * 2^32

const float kNoiseClockRatio = 32.0f;

inline int MixBits(int zone, int b1, int b2) {
  return zone == 0 ? (b1 ^ b2) : zone == 1 ? (b1 | b2) : (b1 & b2);
}

}  // namespace

void ZxPhase48k::Init(stmlib::BufferAllocator* allocator) {
  zone_ = 0;
  interval_ = 0;
  noise_mode_ = false;
  dc_main_ = 0.0f;
  dc_aux_ = 0.0f;
  for (int i = 0; i < 16; ++i) {
    feedback_bits_[i] = 0;
  }
  feedback_pos_ = 0;
  kick_ = 0.0f;
  Reset();
}

void ZxPhase48k::Reset() {
  phase1_ = 0;
  phase2_ = 0x40000000u;  // quarter-cycle offset: strikes never start locked
  ghost_phase_ = 0;
  lfsr_ = 0xACE1u;
  tick_.Init(pulse::kFrameTickPeriod);
  duty2_index_ = -1;      // resynced to the base duty on next Render
  sweep_direction_ = 1;
  sweep_count_ = 0;
}

void ZxPhase48k::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  const bool struck = (parameters.trigger & TRIGGER_RISING_EDGE) != 0;
  if (struck) {
    Reset();
  }

  const float raw_zone = parameters.harmonics * 3.0f;
  int candidate = static_cast<int>(raw_zone);
  CONSTRAIN(candidate, 0, 2);
  if (candidate != zone_ &&
      (raw_zone > static_cast<float>(zone_) + 1.08f ||
       raw_zone < static_cast<float>(zone_) - 0.08f)) {
    zone_ = candidate;
  }
  float zone_position = raw_zone - static_cast<float>(zone_);
  CONSTRAIN(zone_position, 0.0f, 1.0f);

  const bool feedback_on = zone_position > 0.15f;
  uint32_t feedback_delay = 2;
  if (feedback_on) {
    const float p = (zone_position - 0.15f) * (1.0f / 0.85f);
    feedback_delay = static_cast<uint32_t>(
        512.0f * SemitonesToRatio(-72.0f * p));  // 512 .. 8 internal samples
    if (feedback_delay < 2) {
      feedback_delay = 2;
    }
  }

  const float pwm = parameters.timbre;
  const bool sweep_on = pwm > 0.08f;
  float ps = sweep_on ? (pwm - 0.08f) * (1.0f / 0.92f) : 0.0f;
  CONSTRAIN(ps, 0.0f, 1.0f);
  const int sweep_min = 14 - static_cast<int>(ps * 12.99f);   // 14 .. 2
  const int sweep_period = 8 - static_cast<int>(ps * 7.0f);   // 8 .. 1 ticks
  const int base_index = 14;  // both oscillators rest at fat 4-bit squares
  const uint32_t duty1 =
      static_cast<uint32_t>(static_cast<float>(base_index) *
                            (1.0f / 32.0f) * kTwo32);
  if (duty2_index_ < 0 || !sweep_on) {
    duty2_index_ = base_index;
  }

  const float morph = parameters.morph;
  if (!noise_mode_ && morph > 0.92f) {
    noise_mode_ = true;
  } else if (noise_mode_ && morph < 0.88f) {
    noise_mode_ = false;
  }
  float detune_semitones;
  if (morph < 0.25f) {
    detune_semitones = morph * (1.0f / 0.25f) * 0.30f;
  } else {
    static const float kIntervals[6] = {
      0.0f, 5.0f, 7.0f, 12.0f, 19.0f, 24.0f };
    float x = (morph - 0.25f) * (1.0f / 0.65f);
    CONSTRAIN(x, 0.0f, 1.0f);
    const float raw_step = x * 5.99f;
    int step = static_cast<int>(raw_step);
    CONSTRAIN(step, 0, 5);
    if (step != interval_ &&
        (raw_step > static_cast<float>(interval_) + 1.08f ||
         raw_step < static_cast<float>(interval_) - 0.08f)) {
      interval_ = step;
    }
    detune_semitones = kIntervals[interval_] + 0.06f;
  }

  const float macro_value = 0.5f;
  const float kick_depth = macro_value < 0.5f
      ? (0.5f - macro_value) * 2.0f * 24.0f
      : 0.0f;
  if (struck && kick_depth > 0.5f) {
    kick_ = kick_depth;
  }

  float dt1 = NoteToFrequency(parameters.note - 24.0f + kick_);
  kick_ *= 0.9835f;  // ~15 ms fall
  if (kick_ < 0.05f) {
    kick_ = 0.0f;
  }
  CONSTRAIN(dt1, 0.0f, 0.2f);
  float dt2 = dt1 * SemitonesToRatio(detune_semitones);
  if (noise_mode_) {
    dt2 *= kNoiseClockRatio;
  }
  CONSTRAIN(dt2, 0.0f, 0.24f);
  uint32_t increment1 = static_cast<uint32_t>(dt1 * kTwo30);
  uint32_t increment2 = static_cast<uint32_t>(dt2 * kTwo30);

  const uint32_t ghost_duty = macro_value > 0.5f
      ? static_cast<uint32_t>((macro_value * 2.0f - 1.0f) * 0.35f * kTwo32)
      : 0u;

  for (size_t i = 0; i < size; ++i) {
    if (tick_.Next() && sweep_on) {
      ++sweep_count_;
      if ((sweep_count_ % static_cast<uint32_t>(sweep_period)) == 0) {
        duty2_index_ += sweep_direction_;
        if (duty2_index_ >= 14) {
          duty2_index_ = 14;
          sweep_direction_ = -1;
        } else if (duty2_index_ <= sweep_min) {
          duty2_index_ = sweep_min;
          sweep_direction_ = 1;
        }
      }
    }
    const uint32_t duty2 =
        static_cast<uint32_t>(static_cast<float>(duty2_index_) *
                              (1.0f / 32.0f) * kTwo32);
    int acc = 0;
    int acc2 = 0;
    for (int k = 0; k < 4; ++k) {
      phase1_ += increment1;
      const uint32_t previous2 = phase2_;
      phase2_ += increment2;
      if (phase2_ < previous2) {
        const uint16_t feedback = (lfsr_ ^ (lfsr_ >> 1)) & 1u;
        lfsr_ = static_cast<uint16_t>((lfsr_ >> 1) | (feedback << 14));
      }
      const int b1 = phase1_ < duty1 ? 1 : 0;
      const int b2 = noise_mode_
          ? static_cast<int>(lfsr_ & 1u)
          : (phase2_ < duty2 ? 1 : 0);
      int mixed = MixBits(zone_, b1, b2);
      if (feedback_on) {
        const uint32_t tap = (feedback_pos_ - feedback_delay) & 511u;
        mixed ^= static_cast<int>(
            (feedback_bits_[tap >> 5] >> (tap & 31u)) & 1u);
      }
      const uint32_t wp = feedback_pos_ & 511u;
      feedback_bits_[wp >> 5] =
          (feedback_bits_[wp >> 5] & ~(1u << (wp & 31u))) |
          (static_cast<uint32_t>(mixed) << (wp & 31u));
      ++feedback_pos_;
      if (ghost_duty) {
        ghost_phase_ += kGhostIncrement;
        mixed ^= ghost_phase_ < ghost_duty ? 1 : 0;
      }
      acc += mixed;
      acc2 += b2;
    }
    float v = static_cast<float>(acc) * 0.5f - 1.0f;
    float v2 = static_cast<float>(acc2) * 0.5f - 1.0f;
    dc_main_ += 0.002f * (v - dc_main_);
    dc_aux_ += 0.002f * (v2 - dc_aux_);
    const float main = 0.6f * (v - dc_main_);

    out[i] = main;
    aux[i] = 0.6f * (v2 - dc_aux_);

  }
}

}  // namespace plaits
