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
// Snare: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/snare_engine.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/utils/random.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {
// Braids' coefficient table uses 96 kHz while the resonators run at 48 kHz.
inline float SvfFrequencyCoefficient(float note_semitones) {
  const float cutoff_hz = NoteToFrequency(note_semitones) * kCorrectedSampleRate;
  float f_norm = cutoff_hz / kSnareSvfLutSampleRate;
  if (f_norm > kSnareSvfMaxNormalizedFrequency) {
    f_norm = kSnareSvfMaxNormalizedFrequency;
  }
  return 2.0f * sinf(float(M_PI) * f_norm);
}

inline float SvfResonanceDomain(float resonance_code) {
  return resonance_code * (1.0f / 128.0f) * (1.0f / 260.0f);
}

inline float SvfDamp(float resonance_domain) {
  float damp = 2.0f * (1.0f - Sqrt(Sqrt(resonance_domain)));
  if (damp > 2.0f) {
    damp = 2.0f;
  }
  return damp;
}

inline float SvfStep(float in, float f, float damp, float& lp, float& bp) {
  const float notch = in - bp * damp;
  lp += f * bp;
  CONSTRAIN(lp, -kSnareClip, kSnareClip);
  const float hp = notch - lp;
  bp += f * hp;
  CONSTRAIN(bp, -kSnareClip, kSnareClip);
  return bp;
}

struct SnarePulse {
  float state;
  int32_t counter;
  float level;

  inline void Reset() {
    state = 0.0f;
    counter = 0;
    level = 0.0f;
  }

  inline void Trigger(int32_t delay_ticks, float lvl) {
    counter = delay_ticks + 1;
    level = lvl;
  }

  inline bool Done() const {
    return counter == 0;
  }

  // Truncation in Braids' raw units ends the slowly decaying noise envelope.
  inline float Process(float decay) {
    state = float(int32_t(state * 32768.0f * decay)) * (1.0f / 32768.0f);
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

void Snare::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void Snare::Reset() {
  ever_struck_ = false;

  pulse0_state_ = 0.0f;
  pulse0_counter_ = 0;
  pulse0_level_ = 0.0f;
  pulse1_state_ = 0.0f;
  pulse1_counter_ = 0;
  pulse1_level_ = 0.0f;
  pulse2_state_ = 0.0f;
  pulse2_counter_ = 0;
  pulse2_level_ = 0.0f;
  pulse3_state_ = 0.0f;
  pulse3_counter_ = 0;
  pulse3_level_ = 0.0f;
  pulse3_decay_ = kSnarePulse3DecayBase * kSnarePulse3DecayNormalize;

  svf0_lp_ = 0.0f;
  svf0_bp_ = 0.0f;
  svf1_lp_ = 0.0f;
  svf1_bp_ = 0.0f;
  svf2_lp_ = 0.0f;
  svf2_bp_ = 0.0f;

  tone0_resonance_domain_ = SvfResonanceDomain(kSnareTone0ResonanceBase);
  tone1_resonance_domain_ = SvfResonanceDomain(kSnareTone1ResonanceBase);

  gain0_ = kSnareBalanceBase;
  gain1_ = kSnareBalanceBase;
  tone_mix_gain_ = 1.0f;
  noise_mix_gain_ = 1.0f;
  partial_spread_ = 1.0f;
}

void Snare::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = true;

  const bool unpatched = (parameters.trigger & TRIGGER_UNPATCHED) != 0;
  const bool rising_edge = (parameters.trigger & TRIGGER_RISING_EDGE) != 0;

  // An unpatched gate strikes once after Reset.
  const bool self_strike = unpatched && !ever_struck_;
  const bool strike = rising_edge || self_strike;

  float note = parameters.note;
  CONSTRAIN(note, kSnareMinNote, kSnareMaxNote);

  SnarePulse pulse0;
  pulse0.state = pulse0_state_;
  pulse0.counter = pulse0_counter_;
  pulse0.level = pulse0_level_;
  SnarePulse pulse1;
  pulse1.state = pulse1_state_;
  pulse1.counter = pulse1_counter_;
  pulse1.level = pulse1_level_;
  SnarePulse pulse2;
  pulse2.state = pulse2_state_;
  pulse2.counter = pulse2_counter_;
  pulse2.level = pulse2_level_;
  SnarePulse pulse3;
  pulse3.state = pulse3_state_;
  pulse3.counter = pulse3_counter_;
  pulse3.level = pulse3_level_;
  float pulse3_decay = pulse3_decay_;
  float tone0_resonance_domain = tone0_resonance_domain_;
  float tone1_resonance_domain = tone1_resonance_domain_;

  // Resonance and noise decay are snapshots; mix controls stay live.
  if (strike) {
    const float pitch_q7 = note * 128.0f;
    float decay = kSnareDecayPitchBase - pitch_q7;

    const float color_q15 = floorf(parameters.harmonics * 32767.0f);
    if (color_q15 >= kSnareColorDecayThreshold) {
      decay += color_q15 - kSnareColorDecayThreshold;
    }
    if (decay > kSnareDecayMax) {
      decay = kSnareDecayMax;
    }

    tone0_resonance_domain = SvfResonanceDomain(
        kSnareTone0ResonanceBase +
        floorf(decay * kSnareResonanceDecayScale));
    tone1_resonance_domain = SvfResonanceDomain(
        kSnareTone1ResonanceBase +
        floorf(decay * kSnareResonanceDecayScale));
    pulse3_decay = (kSnarePulse3DecayBase +
        floorf(decay * kSnarePulse3DecayScale)) *
        kSnarePulse3DecayNormalize;

    pulse0.Trigger(kSnarePulse0DelayTicks, kSnarePulse0Level);
    pulse1.Trigger(kSnarePulse1DelayTicks, kSnarePulse1Level);
    pulse2.Trigger(kSnarePulse2DelayTicks, kSnarePulse2Level);

    float snappy = color_q15;
    if (snappy > kSnareSnappyCeiling) {
      snappy = kSnareSnappyCeiling;
    }
    pulse3.Trigger(kSnarePulse3DelayTicks,
        kSnarePulse3LevelBase + snappy * kSnarePulse3LevelSlope);

    ever_struck_ = true;
  }

  const float target_gain0 = kSnareBalanceBase -
      parameters.timbre * kSnareBalanceSpan;
  const float target_gain1 = kSnareBalanceBase +
      parameters.timbre * kSnareBalanceSpan;

  float target_tone_mix_gain = 2.0f * (1.0f - parameters.morph);
  if (target_tone_mix_gain > 1.0f) {
    target_tone_mix_gain = 1.0f;
  }
  float target_noise_mix_gain = 2.0f * parameters.morph;
  if (target_noise_mix_gain > 1.0f) {
    target_noise_mix_gain = 1.0f;
  }

  partial_spread_ = 1.0f;

  const float f0 = SvfFrequencyCoefficient(
      note + kSnareTone0SemitoneOffset * partial_spread_);
  const float f1 = SvfFrequencyCoefficient(
      note + kSnareTone1SemitoneOffset * partial_spread_);
  const float f2 = SvfFrequencyCoefficient(note + kSnareNoiseSemitoneOffset);

  ParameterInterpolator gain0_modulation(&gain0_, target_gain0, size);
  ParameterInterpolator gain1_modulation(&gain1_, target_gain1, size);
  ParameterInterpolator tone_mix_modulation(
      &tone_mix_gain_, target_tone_mix_gain, size);
  ParameterInterpolator noise_mix_modulation(
      &noise_mix_gain_, target_noise_mix_gain, size);

  float svf0_lp = svf0_lp_;
  float svf0_bp = svf0_bp_;
  float svf1_lp = svf1_lp_;
  float svf1_bp = svf1_bp_;
  float svf2_lp = svf2_lp_;
  float svf2_bp = svf2_bp_;

  const float damp0 = SvfDamp(tone0_resonance_domain);
  const float damp1 = SvfDamp(tone1_resonance_domain);
  const float damp2 = SvfDamp(SvfResonanceDomain(kSnareNoiseResonanceCode));

  while (size--) {
    const float gain0 = gain0_modulation.Next();
    const float gain1 = gain1_modulation.Next();
    const float tone_mix_gain = tone_mix_modulation.Next();
    const float noise_mix_gain = noise_mix_modulation.Next();

    const float pulse0_out = pulse0.Process(kSnarePulse0Decay);
    const float pulse1_out = pulse1.Process(kSnarePulse1Decay);
    const bool pulse1_active = !pulse1.Done();
    const float excitation0 = pulse0_out + pulse1_out +
        (pulse1_active ? kSnarePulse1Hold : 0.0f);

    const float pulse2_out = pulse2.Process(kSnarePulse2Decay);
    const bool pulse2_active = !pulse2.Done();
    const float excitation1 = pulse2_out +
        (pulse2_active ? kSnarePulse2Hold : 0.0f);

    const float random_sample = Random::GetSample() * (1.0f / 32768.0f);
    const float pulse3_out = pulse3.Process(pulse3_decay);
    const float noise_sample = random_sample * pulse3_out;

    float sample_f0 = f0;
    float sample_f1 = f1;
    float sample_f2 = f2;
    const float tone0 = SvfStep(
        excitation0, sample_f0, damp0, svf0_lp, svf0_bp) +
        excitation0 * kSnareDryMix;
    const float tone1 = SvfStep(
        excitation1, sample_f1, damp1, svf1_lp, svf1_bp) +
        excitation1 * kSnareDryMix;
    const float noise = SvfStep(
        noise_sample, sample_f2, damp2, svf2_lp, svf2_bp);

    const float tone_sum = tone0 * gain0 + tone1 * gain1;
    float sample = tone_sum * tone_mix_gain + noise * noise_mix_gain;
    CONSTRAIN(sample, -kSnareClip, kSnareClip);

    *out++ = sample;
    *aux++ = SoftClip(noise);
  }

  pulse0_state_ = pulse0.state;
  pulse0_counter_ = pulse0.counter;
  pulse0_level_ = pulse0.level;
  pulse1_state_ = pulse1.state;
  pulse1_counter_ = pulse1.counter;
  pulse1_level_ = pulse1.level;
  pulse2_state_ = pulse2.state;
  pulse2_counter_ = pulse2.counter;
  pulse2_level_ = pulse2.level;
  pulse3_state_ = pulse3.state;
  pulse3_counter_ = pulse3.counter;
  pulse3_level_ = pulse3.level;
  pulse3_decay_ = pulse3_decay;
  tone0_resonance_domain_ = tone0_resonance_domain;
  tone1_resonance_domain_ = tone1_resonance_domain;

  svf0_lp_ = svf0_lp;
  svf0_bp_ = svf0_bp;
  svf1_lp_ = svf1_lp;
  svf1_bp_ = svf1_bp;
  svf2_lp_ = svf2_lp;
  svf2_bp_ = svf2_bp;
}

}  // namespace plaits
