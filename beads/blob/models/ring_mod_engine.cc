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
//
//
// Ring Mod: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/ring_mod_engine.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

inline float RingModOverdrive(float x) {
  CONSTRAIN(x, -1.0f, 1.0f);
  return kRingModShaperNorm * SoftLimit(2.0f * x);
}

inline float BraidsDetune(float knob) {
  const int parameter = static_cast<int>(knob * 32767.0f);
  int units = parameter - 16384;
  units = units >= 0 ? (units >> 2) : -((-units + 3) >> 2);
  return static_cast<float>(units) * (1.0f / 128.0f);
}

}  // namespace

// Palette's halfband filter keeps the original two-times render.
inline float RingMod::Decimate(const float* history) const {
  const int p = history_position_;
  return kRingModHalfbandCentre * history[(p - 7) & 15]
      + kRingModHalfband1 * (history[(p - 6) & 15] + history[(p - 8) & 15])
      + kRingModHalfband3 * (history[(p - 4) & 15] + history[(p - 10) & 15])
      + kRingModHalfband5 * (history[(p - 2) & 15] + history[(p - 12) & 15])
      + kRingModHalfband7 * (history[p] + history[(p - 14) & 15]);
}

void RingMod::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void RingMod::Reset() {
  phase_ = 0.0f;          // Braids' local carrier phase 0.25, plus the 0.75 offset
  modulator_phase_ = 0.75f;
  modulator_phase_2_ = 0.75f;
  for (int i = 0; i < 16; ++i) {
    history_[i] = 0.0f;
    history_aux_[i] = 0.0f;
  }
  history_position_ = 0;
}

void RingMod::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    phase_ = 0.75f;
    modulator_phase_ = 0.75f;
    modulator_phase_2_ = 0.75f;
  }

  float carrier_note = parameters.note;
  if (carrier_note > kRingModMaxPitch) {
    carrier_note = kRingModMaxPitch;
  }
  float note_1 = parameters.note + BraidsDetune(parameters.harmonics);
  float note_2 = parameters.note + BraidsDetune(parameters.timbre);
  if (note_1 > kRingModMaxPitch) {
    note_1 = kRingModMaxPitch;
  }
  if (note_2 > kRingModMaxPitch) {
    note_2 = kRingModMaxPitch;
  }

  const float increment = NoteToFrequency(carrier_note) * 0.5f;
  const float increment_1 = NoteToFrequency(note_1) * 0.5f;
  const float increment_2 = NoteToFrequency(note_2) * 0.5f;
  const float carrier_frequency = increment * 2.0f;
  const float ratio_1 = increment_1 /
      (increment > 1.0e-9f ? increment : 1.0e-9f);
  const float ratio_2 = increment_2 /
      (increment > 1.0e-9f ? increment : 1.0e-9f);

  const float depth = parameters.morph;
  const float drive = 1.0f;
  const bool modulated = depth > 0.001f;

  for (size_t i = 0; i < size; ++i) {
    const float root_offset = 0.0f;
    float carrier_step = (carrier_frequency + root_offset) * 0.5f;
    float modulator_step_1 = increment_1 + root_offset * 0.5f * ratio_1;
    float modulator_step_2 = increment_2 + root_offset * 0.5f * ratio_2;
    CONSTRAIN(carrier_step, -0.5f, 0.499999f);
    CONSTRAIN(modulator_step_1, -0.5f, 0.499999f);
    CONSTRAIN(modulator_step_2, -0.5f, 0.499999f);
    for (int s = 0; s < 2; ++s) {
      phase_ += carrier_step;
      if (phase_ >= 1.0f) {
        phase_ -= 1.0f;
      } else if (phase_ < 0.0f) {
        phase_ += 1.0f;
      }
      modulator_phase_ += modulator_step_1;
      if (modulator_phase_ >= 1.0f) {
        modulator_phase_ -= 1.0f;
      } else if (modulator_phase_ < 0.0f) {
        modulator_phase_ += 1.0f;
      }
      modulator_phase_2_ += modulator_step_2;
      if (modulator_phase_2_ >= 1.0f) {
        modulator_phase_2_ -= 1.0f;
      } else if (modulator_phase_2_ < 0.0f) {
        modulator_phase_2_ += 1.0f;
      }

      const float carrier = SineNoWrap(phase_);
      float ring = carrier;
      float ring_aux = carrier;
      if (modulated) {
        const float m1 = 1.0f + depth * (SineNoWrap(modulator_phase_) - 1.0f);
        const float m2 = 1.0f + depth * (SineNoWrap(modulator_phase_2_) - 1.0f);
        ring_aux = carrier * m1;
        ring = ring_aux * m2;
      }

      history_position_ = (history_position_ + 1) & 15;
      history_[history_position_] =
          RingModOverdrive(kRingModPreShaperScale * drive * ring);

    }

    out[i] = Decimate(history_);
    aux[i] = kRingModAuxGain * SineNoWrap(modulator_phase_);

  }
}

}  // namespace plaits
