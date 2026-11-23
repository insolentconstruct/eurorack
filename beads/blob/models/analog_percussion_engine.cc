// Copyright 2026 Dylan Bolink.
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
// AnalogPercussion: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/analog_percussion_engine.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"

#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/units.h"

namespace plaits {

using namespace stmlib;

namespace {

inline float ApplyMacro(
    float stock_value,
    float minimum_value,
    float maximum_value,
    float macro) {
  const float amount = macro * 2.0f;
  return macro < 0.5f
      ? minimum_value + (stock_value - minimum_value) * amount
      : stock_value + (maximum_value - stock_value) * (amount - 1.0f);
}

}  // namespace

// Leave headroom for two sine modes before soft clipping.
const float kPercussionMakeup = 0.78f;

const float kPercussionOvertoneLoad = 0.55f;
const float kPercussionClickLoad = 0.45f;

// AUX carries one sine and needs no headroom for a second mode or a click.
const float kPercussionMakeupAux = 0.8f;

// The click's own trim.
const float kPercussionClickMakeup = 6.0f;

// Fade the second mode over the top audible octave.
const float kPercussionNyquistFadeStart = 0.40f;
const float kPercussionNyquistFadeEnd = 0.48f;

void AnalogPercussion::Init(BufferAllocator* allocator) {
  click_filter_.Init();
  shape_band_.Init();
  shape_band_aux_.Init();
  Reset();
}

void AnalogPercussion::Reset() {
  click_filter_.Reset();
  shape_band_.Reset();
  shape_band_aux_.Reset();
  shape_amount_ = 0.0f;
  shape_wave_ = 0.0f;
  shape_bias_ = 0.0f;
  shape_dc_ = 0.0f;
  shape_drive_ = 1.0f;
  shape_norm_ = 1.0f;
  for (int i = 0; i < 2; ++i) {
    phase_[i] = 0.0f;
    phase_increment_[i] = 0.0f;
    mode_decay_[i] = 0.0f;
    mode_env_[i] = 0.0f;
  }
  twin_phase_ = 0.0f;
  twin_increment_ = 0.0f;
  sub_phase_ = 0.0f;
  sub_increment_ = 0.0f;
  sub_decay_ = 0.0f;
  sub_env_ = 0.0f;
  fast_env_ = 0.0f;
  fast_decay_ = 0.0f;
  click_env_ = 0.0f;
  click_decay_ = 0.0f;
  click_level_ = 0.0f;
  prev_gain_ = 0.0f;
  prev_gain_aux_ = 0.0f;
  sustain_gain_ = 0.0f;
}

void AnalogPercussion::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  const bool sustain = parameters.trigger & TRIGGER_UNPATCHED;
  const bool trigger = parameters.trigger & TRIGGER_RISING_EDGE;

  const float f0 = NoteToFrequency(parameters.note);

  // One monotonic material axis; see kPercussionShapeStart in the header.
  float overtone_position = parameters.timbre / kPercussionShapeStart;
  if (overtone_position > 1.0f) {
    overtone_position = 1.0f;
  }
  shape_amount_ = (parameters.timbre - kPercussionShapeStart) /
      (1.0f - kPercussionShapeStart);
  CONSTRAIN(shape_amount_, 0.0f, 1.0f);

  const float migration =
      shape_amount_ * shape_amount_ * shape_amount_;
  const float stock_ratio = kPercussionStockRatio * (1.0f - migration) +
      kPercussionCowbellRatio * migration;
  const float ratio = ApplyMacro(stock_ratio, kPercussionMinRatio,
      kPercussionMaxRatio, 0.5f);
  const float cycles = ApplyMacro(kPercussionStockCycles, kPercussionMinCycles,
      kPercussionMaxCycles, parameters.morph);

  phase_increment_[0] = f0;
  phase_increment_[1] = f0 * ratio;
  sub_increment_ = f0 * kPercussionSubRatio;
  twin_increment_ = f0 *
      SemitonesToRatio(kPercussionStereoDetuneCents * 0.01f);

  for (int i = 0; i < 2; ++i) {
    float f = phase_increment_[i];
    if (f < 1.0e-5f) {
      f = 1.0e-5f;
    }
    const float tau = cycles / (kPercussionDecibelSpan * f);
    mode_decay_[i] = 1.0f - 1.0f / tau;
    if (mode_decay_[i] < 0.0f) {
      mode_decay_[i] = 0.0f;
    }
  }

  {
    float f = sub_increment_;
    if (f < 1.0e-5f) {
      f = 1.0e-5f;
    }
    const float tau = cycles / (kPercussionDecibelSpan * f);
    sub_decay_ = 1.0f - 1.0f / tau;
    if (sub_decay_ < 0.0f) {
      sub_decay_ = 0.0f;
    }
  }

  const float mode_2_level =
      (overtone_position * overtone_position * kPercussionOvertoneMax) *
      (1.0f - shape_amount_) +
      kPercussionOvertoneCowbell * shape_amount_;

  const float upper_hz = phase_increment_[1] * kSampleRate;
  float pitch_taper = (kPercussionShapeSilentHz - upper_hz) /
      (kPercussionShapeSilentHz - kPercussionShapeFullHz);
  CONSTRAIN(pitch_taper, 0.0f, 1.0f);
  shape_wave_ = shape_amount_ * pitch_taper;

  shape_drive_ = 1.0f + shape_wave_ * (kPercussionShapeDrive - 1.0f);

  shape_bias_ = kPercussionShapeBias * shape_wave_;
  shape_dc_ = SoftClip(shape_bias_);
  shape_norm_ = 1.0f / SoftClip(shape_drive_);
  float band_f = f0 * ratio * kPercussionBandRatio;
  CONSTRAIN(band_f, 0.002f, 0.45f);
  shape_band_.set_f_q<FREQUENCY_FAST>(band_f, kPercussionBandQ);
  shape_band_aux_.set(shape_band_);

  // HARMONICS click
  click_level_ = parameters.harmonics;

  float click_f = f0 * kPercussionClickRatio;
  CONSTRAIN(click_f, 0.002f, 0.45f);
  click_filter_.set_f_q<FREQUENCY_FAST>(click_f, kPercussionClickQ);
  const float click_tau = kPercussionClickCycles /
      (kPercussionDecibelSpan * click_f);
  click_decay_ = 1.0f - 1.0f / click_tau;
  if (click_decay_ < 0.0f) {
    click_decay_ = 0.0f;
  }

  const float fast_tau = kPercussionFastTauMs * 1.0e-3f * kSampleRate;
  fast_decay_ = 1.0f - 1.0f / fast_tau;
  if (fast_decay_ < 0.0f) {
    fast_decay_ = 0.0f;
  }

  const float accent = 0.3f + 0.7f * parameters.accent;

  if (trigger) {
    mode_env_[0] = accent;
    mode_env_[1] = accent * mode_2_level;
    click_env_ = accent * click_level_;
    fast_env_ = accent * kPercussionFastLevel * shape_amount_;
    sub_env_ = accent;
    // Both modes restart in phase.
    phase_[0] = 0.0f;
    phase_[1] = 0.0f;
    sub_phase_ = 0.0f;
    // The twin starts a quarter cycle ahead.
    twin_phase_ = kPercussionTwinPhaseOffset;
  }

  float mode_2_gain = (kPercussionNyquistFadeEnd - phase_increment_[1]) /
      (kPercussionNyquistFadeEnd - kPercussionNyquistFadeStart);
  CONSTRAIN(mode_2_gain, 0.0f, 1.0f);
  mode_2_gain = mode_2_gain * mode_2_gain * (3.0f - 2.0f * mode_2_gain);
  const float audible_mode_2_level = mode_2_level * mode_2_gain;

  // MORPH scales the drone's level, which is what all four stock drums do
  ParameterInterpolator sustain_level(&sustain_gain_,
      sustain ? accent * parameters.morph : 0.0f, size);

  const float gain = kPercussionMakeup / (1.0f +
      kPercussionOvertoneLoad * audible_mode_2_level +
      kPercussionClickLoad * click_level_);
  ParameterInterpolator envelope_gain(&prev_gain_, gain, size);
  ParameterInterpolator envelope_gain_aux(&prev_gain_aux_, kPercussionMakeupAux,
      size);

  // Fade VCA saturation in with the shaping.
  const float slow_duck = 1.0f - shape_amount_ * kPercussionSlowDuck;

  const float vca_drive =
      1.0f + shape_amount_ * (kPercussionVcaDrive - 1.0f);
  const float vca_trim = 1.0f / SoftClip(vca_drive);

  for (size_t i = 0; i < size; ++i) {
    const float drone = sustain_level.Next();

    float fundamental_increment = phase_increment_[0];
    float overtone_increment = phase_increment_[1];
    float sub_increment = sub_increment_;

    phase_[0] += fundamental_increment;
    phase_[0] -= static_cast<int>(phase_[0]);
    phase_[1] += overtone_increment;
    phase_[1] -= static_cast<int>(phase_[1]);
    sub_phase_ += sub_increment;
    sub_phase_ -= static_cast<int>(sub_phase_);

    mode_env_[0] *= mode_decay_[0];
    mode_env_[1] *= mode_decay_[1];
    click_env_ *= click_decay_;
    fast_env_ *= fast_decay_;
    sub_env_ *= sub_decay_;

    // Free-running
    const float env_0 = drone > mode_env_[0] ? drone : mode_env_[0];
    const float env_1 = drone * mode_2_level > mode_env_[1]
        ? drone * mode_2_level
        : mode_env_[1];
    const float click_drone =
        drone * click_level_ * kPercussionClickDroneTrim;
    const float env_click = click_drone > click_env_ ? click_drone : click_env_;

    const float env_sub = drone > sub_env_ ? drone : sub_env_;

    const float fundamental =
        Shape(Sine(phase_[0])) * (env_0 * slow_duck + fast_env_);
    const float overtone =
        Shape(Sine(phase_[1])) * (env_1 * slow_duck + fast_env_) * mode_2_gain;

    float click = 2.0f * Random::GetFloat() - 1.0f;
    click = click_filter_.Process<FILTER_MODE_HIGH_PASS>(click) * env_click *
        kPercussionClickMakeup;

    const float g = envelope_gain.Next();
    const float g_aux = envelope_gain_aux.Next();

    float tone = fundamental + overtone;
    if (shape_amount_ > 0.0f) {
      const float driven = SoftClip(tone * vca_drive) * vca_trim;
      const float banded =
          shape_band_.Process<FILTER_MODE_HIGH_PASS>(driven);
      tone += (banded - tone) * shape_amount_;
    }
    out[i] = SoftClip((tone + click) * g);
    // AUX is an octave-down strike on the same trigger.
    aux[i] = SoftClip(Sine(sub_phase_) * env_sub * g_aux);

  }
}

}  // namespace plaits
