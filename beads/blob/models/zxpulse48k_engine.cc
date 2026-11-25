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

#include "beads/blob/models/zxpulse48k_engine.h"

#include <algorithm>

namespace plaits {

using namespace stmlib;

namespace {

const float kVoiceAmp = 0.3f;
const float kEchoAmp = 0.24f;
const float kMaxDt = 0.2f;
const float kTwoOctavesDown = -24.0f;

inline float ClampDuty(float duty, float dt) {
  return std::min(std::max(duty, dt), 0.45f);
}

}  // namespace

void ZxPulse48k::Init(stmlib::BufferAllocator* allocator) {
  chords_.Init(allocator);
  chords_.Reset();
  arp_mode_ = false;
  stack_count_ = 1;
  pull_ = 0.0f;
  Reset();
}

void ZxPulse48k::Reset() {
  for (int v = 0; v < kNumVoices; ++v) {
    channels_[v].Init();
    for (int k = 0; k < v; ++k) {
      float dummy;
      channels_[v].Next(0.13f, 0.13f, &dummy);
    }
  }
  tick_.Init(pulse::kFrameTickPeriod);
  tick_count_ = 0;
  arp_index_ = 0;
  history_pos_ = 0;
  for (int i = 0; i < kHistorySize; ++i) {
    dt_history_[i] = 0.0f;
    width_history_[i] = 0.0f;
  }
}

void ZxPulse48k::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  chords_.set_chord(parameters.harmonics, 0);
  const float* ratios = chords_.ratios();

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    Reset();
  }

  const float width = 0.5f * SemitonesToRatio(79.2f * parameters.timbre);

  const float morph = parameters.morph;
  if (!arp_mode_ && morph > 0.52f) {
    arp_mode_ = true;
  } else if (arp_mode_ && morph < 0.48f) {
    arp_mode_ = false;
  }
  int arp_divider = 1;
  if (arp_mode_) {
    float p = (morph - 0.52f) * (1.0f / 0.48f);
    CONSTRAIN(p, 0.0f, 1.0f);
    arp_divider = 6 - static_cast<int>(p * 5.49f);
  } else {
    const float raw = 1.0f + morph * (1.0f / 0.48f) * 4.49f;
    if (raw > static_cast<float>(stack_count_) + 0.6f ||
        raw < static_cast<float>(stack_count_) - 0.6f) {
      stack_count_ = static_cast<int>(raw);
      CONSTRAIN(stack_count_, 1, kNumVoices);
    }
  }

  const float macro = 0.5f;
  const bool echo_on = macro > 0.55f;
  const float echo_amount = echo_on ? (macro - 0.55f) * (1.0f / 0.45f) : 0.0f;
  const int echo_delay = 2 + static_cast<int>(echo_amount * 9.99f);
  const float echo_width_scale = 0.75f - 0.4f * echo_amount;
  const float pull_depth = macro < 0.45f
      ? 0.03f + (0.45f - macro) * (1.0f / 0.45f) * 0.09f
      : 0.03f;

  const float note = parameters.note + kTwoOctavesDown;
  const float f0 = NoteToFrequency(note);
  float dts[kNumVoices];
  float duties[kNumVoices];
  float amps[kNumVoices];
  int n = 0;
  int arp_slot = -1;
  if (arp_mode_) {
    dts[n] = f0;
    amps[n] = kVoiceAmp;
    ++n;
    dts[n] = f0 * 1.5f;
    amps[n] = kVoiceAmp;
    ++n;
    arp_slot = n;
    dts[n] = f0 * ratios[arp_index_ & 3] * 2.0f;
    amps[n] = kVoiceAmp;
    ++n;
  } else {
    int count = stack_count_;
    if (echo_on && count == kNumVoices) {
      --count;  // Follin: the last slot yields to the echo.
    }
    for (int v = 0; v < count; ++v) {
      dts[n] = v < 4 ? f0 * ratios[v] : f0 * ratios[0] * 2.0f;
      amps[n] = kVoiceAmp;
      ++n;
    }
  }
  const int echo_src = arp_mode_ ? arp_slot : (n > 0 ? n - 1 : 0);
  int echo_slot = -1;
  if (echo_on && n < kNumVoices) {
    echo_slot = n;
    ++n;
  }

  float traffic = 0.0f;
  for (int v = 0; v < n; ++v) {
    if (v == echo_slot) {
      continue;
    }
    CONSTRAIN(dts[v], 0.0f, kMaxDt);
    duties[v] = ClampDuty(width * dts[v], dts[v]);
    traffic += duties[v];
  }
  float load = traffic * (1.0f / 0.15f);
  CONSTRAIN(load, 0.0f, 1.5f);
  pull_ += 0.02f * (pull_depth * load - pull_);
  const float pull_ratio = SemitonesToRatio(-pull_);
  for (int v = 0; v < n; ++v) {
    if (v == echo_slot) {
      continue;
    }
    dts[v] *= pull_ratio;
  }

  if (echo_slot >= 0) {
    const int read = (history_pos_ - echo_delay) & (kHistorySize - 1);
    dts[echo_slot] = dt_history_[read];
    duties[echo_slot] = ClampDuty(
        width_history_[read] * echo_width_scale, dts[echo_slot]);
    amps[echo_slot] = kEchoAmp;
  }

  for (size_t i = 0; i < size; ++i) {
    if (tick_.Next()) {
      dt_history_[history_pos_] = dts[echo_src];
      width_history_[history_pos_] = duties[echo_src];
      history_pos_ = (history_pos_ + 1) & (kHistorySize - 1);
      ++tick_count_;
      if (arp_mode_ && (tick_count_ % static_cast<uint32_t>(arp_divider)) == 0) {
        arp_index_ = (arp_index_ + 1) & 3;
        if (arp_slot >= 0) {
          float dt = f0 * ratios[arp_index_ & 3] * 2.0f * pull_ratio;
          CONSTRAIN(dt, 0.0f, kMaxDt);
          dts[arp_slot] = dt;
          duties[arp_slot] = ClampDuty(width * dt, dt);
        }
      }
    }

      float clean = 0.0f;
      float nasty = 0.0f;
      for (int v = 0; v < n; ++v) {
        float dt = dts[v];
        float duty = duties[v];
        float naive;
        clean += amps[v] * channels_[v].Next(dt, duty, &naive);
        nasty += amps[v] * naive;
      }
      out[i] = clean;
      aux[i] = nasty;

  }
}

}  // namespace plaits
