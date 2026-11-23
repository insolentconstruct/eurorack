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
// Kick: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/kick_engine.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/parameter_interpolator.h"

namespace plaits {

using namespace stmlib;

namespace {

// Braids' timbre polynomial sets the SVF resonance domain.
inline float KickResonanceDomain(float timbre) {
  const float decay = timbre * 32767.0f;
  const float scaled0 = 65535.0f - 2.0f * decay;
  const float squared = scaled0 * scaled0 * (1.0f / 65536.0f);
  const float scaled1 = squared * scaled0 * (1.0f / 262144.0f);
  const float resonance = 32640.0f - scaled1;  // 32768 - 128 - scaled
  // The table domain uses resonance / (128 * 260).
  return resonance * (1.0f / 33280.0f);
}

// Combine Braids' two smoothing steps into one step at 48 kHz.
inline float KickToneCoefficient(float color) {
  const float c = color * 32767.0f;
  const float c1 = c * c * (1.0f / 32768.0f);
  const float c2 = c1 * c1 * (1.0f / 32768.0f);
  const float lp_coefficient = 128.0f + 1.5f * c2;  // 128 + (c2>>1)*3
  const float k = lp_coefficient * (1.0f / 32768.0f);
  const float one_minus_k = 1.0f - k;
  return 1.0f - one_minus_k * one_minus_k;
}

// A retrigger adds a pulse to the current excitation decay.
struct KickPulse {
  float state;
  int32_t counter;

  inline void Reset() {
    state = 0.0f;
    counter = 0;
  }

  inline void Trigger(int32_t delay_ticks) {
    counter = delay_ticks + 1;
  }

  // The hold test uses the counter before this sample is processed.
  inline bool DoneBefore() const {
    return counter == 0;
  }

  // Levels and decay factors are already in the float sample domain.
  inline float Process(float decay, float level) {
    state *= decay;
    if (counter > 0) {
      --counter;
      if (counter == 0) {
        state += fabsf(level);
      }
    }
    return level < 0.0f ? -state : state;
  }
};

}  // namespace

void Kick::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void Kick::Reset() {
  pulse0_state_ = 0.0f;
  pulse0_counter_ = 0;
  pulse1_state_ = 0.0f;
  pulse1_counter_ = 0;
  click_counter_ = 0;
  svf_lp_ = 0.0f;
  svf_bp_ = 0.0f;
  lp_state_ = 0.0f;
  resonance_domain_ = KickResonanceDomain(0.5f);
  tone_coefficient_ = KickToneCoefficient(0.5f);
  punch_scale_ = 1.0f;
  balance_gain0_ = 1.0f;
  balance_gain1_ = 1.0f;
}

void Kick::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = true;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    pulse0_counter_ = kKickPulse0DelayTicks + 1;
    pulse1_counter_ = kKickPulse1DelayTicks + 1;
    click_counter_ = kKickClickDelayTicks + 1;
  }

  const float target_resonance_domain = KickResonanceDomain(parameters.timbre);
  const float target_tone = KickToneCoefficient(parameters.harmonics);
  const float target_punch_scale = 1.0f;

  // The midpoint leaves both excitation pulses at their native levels.
  float target_gain0 = 2.0f * (1.0f - parameters.morph);
  if (target_gain0 > 1.0f) {
    target_gain0 = 1.0f;
  }
  float target_gain1 = 2.0f * parameters.morph;
  if (target_gain1 > 1.0f) {
    target_gain1 = 1.0f;
  }

  ParameterInterpolator resonance_modulation(
      &resonance_domain_, target_resonance_domain, size);
  ParameterInterpolator tone_modulation(&tone_coefficient_, target_tone, size);
  ParameterInterpolator punch_modulation(
      &punch_scale_, target_punch_scale, size);
  ParameterInterpolator gain0_modulation(&balance_gain0_, target_gain0, size);
  ParameterInterpolator gain1_modulation(&balance_gain1_, target_gain1, size);

  KickPulse pulse0 = { pulse0_state_, pulse0_counter_ };
  KickPulse pulse1 = { pulse1_state_, pulse1_counter_ };

  while (size--) {
    // Decrement before testing the pitch-click countdown.
    if (click_counter_ > 0) {
      --click_counter_;
    }
    const bool click_active = click_counter_ > 0;

    const float note = parameters.note +
        (click_active ? kKickClickSemitones : 0.0f);
    float f_norm = NoteToFrequency(note);
    if (f_norm > kKickSvfMaxNormalizedFrequency) {
      f_norm = kKickSvfMaxNormalizedFrequency;
    }
    const float f = 2.0f * sinf(float(M_PI) * f_norm);

    const float resonance_domain = resonance_modulation.Next();
    const float damp_from_resonance =
        2.0f * (1.0f - Sqrt(Sqrt(resonance_domain)));
    const float damp_from_freq_ceiling = 2.0f / f - f * 0.5f;
    float damp = damp_from_resonance < damp_from_freq_ceiling
        ? damp_from_resonance : damp_from_freq_ceiling;
    if (damp > 2.0f) {
      damp = 2.0f;
    }

    const float punch_scale = punch_modulation.Next();
    float f_eff = f;
    float damp_eff = damp;
    if (punch_scale > 0.0f) {
      const float punch_signal = svf_lp_ > kKickPunchThreshold
          ? svf_lp_ : kKickPunchFloor;
      f_eff += punch_signal * kKickPunchStockCoefficient *
          kKickPunchFreqScale * punch_scale;
      damp_eff += (punch_signal - kKickPunchFloor) *
          kKickPunchDampScale * punch_scale;
    }

    const float gain0 = gain0_modulation.Next();
    const float gain1 = gain1_modulation.Next();

    const bool pulse1_done_before = pulse1.DoneBefore();
    const float pulse0_out = pulse0.Process(kKickPulse0Decay, kKickPulse0Level);
    const float pulse1_hold = pulse1_done_before ? 0.0f : kKickPulse1Hold;
    const float pulse1_out = pulse1.Process(kKickPulse1Decay, kKickPulse1Level);

    const float excitation =
        pulse0_out * gain0 + (pulse1_hold + pulse1_out) * gain1;

    // The Chamberlin recursion (svf.h:91-96), in float.
    const float notch = excitation - svf_bp_ * damp_eff;
    svf_lp_ += f_eff * svf_bp_;
    CONSTRAIN(svf_lp_, -kKickClip, kKickClip);
    const float hp = notch - svf_lp_;
    svf_bp_ += f_eff * hp;
    CONSTRAIN(svf_bp_, -kKickClip, kKickClip);

    const float resonator_output =
        excitation * kKickExcitationDryMix + svf_bp_;

    const float tone = tone_modulation.Next();
    lp_state_ += (resonator_output - lp_state_) * tone;
    CONSTRAIN(lp_state_, -kKickClip, kKickClip);

    const float sample = lp_state_;

    *out++ = sample;
    *aux++ = SoftClip(excitation);
  }

  pulse0_state_ = pulse0.state;
  pulse0_counter_ = pulse0.counter;
  pulse1_state_ = pulse1.state;
  pulse1_counter_ = pulse1.counter;
}

}  // namespace plaits
