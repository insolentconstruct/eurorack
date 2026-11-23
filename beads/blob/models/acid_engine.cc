// Copyright 2026 Dylan Bolink.
// Copyright 2009 Robin Schmidt (www.rs-met.com) — the resonance taper comes from Open303,
// MIT licensed.
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

#include "beads/blob/models/acid_engine.h"

#include "stmlib/dsp/parameter_interpolator.h"

namespace plaits {

using namespace stmlib;

namespace {

inline void MacroToWaveform(float macro, float* waveshape, float* pw) {
  if (macro < 0.5f) {
    *waveshape = 1.0f - macro;         // 1.0 (pulse) .. 0.5 (saw)
    *pw = 0.12f + macro * 0.76f;       // 12% .. 50%
  } else {
    *waveshape = macro;                // 0.5 (saw) .. 1.0 (square)
    *pw = 0.5f;
  }
}

const float kMaxResonance = 4.78f;

const float kMinCutoff = 20.0f;
const float kCutoffRange = 88.0f;

const float kAccentBrightness = 12.0f;
const float kAccentSweepBrightness = 10.0f;

const float kKeyboardTracking = 0.5f;
const float kTrackingCenter = 48.0f;

const float kMaxCutoffFrequency = 0.18f;

const float kBassMakeup = 0.30f;

#ifndef PLAITS_ACID_SWEEP_CHARGE_TIME
#define PLAITS_ACID_SWEEP_CHARGE_TIME 0.040f
#endif
const float kAccentSweepChargeTime = PLAITS_ACID_SWEEP_CHARGE_TIME;
const float kAccentSweepTime = 0.25f;

const float kPreDriveScale = 0.4f;

const float kMainScale = 0.68f;

const float kMainReference = kMainScale * 0.85f;
const float kAuxSaturation = 0.45f;
const float kAuxScale = 0.88f;

#ifndef PLAITS_ACID_STEREO_OFFSET
#define PLAITS_ACID_STEREO_OFFSET 3.0f
#endif
const float kStereoCutoffOffset = PLAITS_ACID_STEREO_OFFSET;

const float kDriveGain = 9.0f;

const float kOscillatorMaxFrequency = 0.25f;

inline float MainScale(float pw, float amount) {
  const float clip_mean = (1.0f - pw) * kAcidClipPositive - pw;
  const float positive = kAcidClipPositive - clip_mean;
  const float negative = 1.0f + clip_mean;
  const float clip_peak = positive > negative ? positive : negative;
  float clipping = amount;
  CONSTRAIN(clipping, 0.0f, 1.0f);
  return kMainScale + (kMainReference / clip_peak - kMainScale) * clipping;
}

const float kInverseOneMinusEMinus3 = 1.052399f;   // 1 / (1 - e^-3)

inline float SkewResonance(float r) {
  return (1.0f - SemitonesToRatio(-51.937f * r)) * kInverseOneMinusEMinus3;
}

inline float ThresholdShape(float f) {
  return 1.0f + kAcidFeedbackHighpass / f;
}

}  // namespace

void Acid::Init(stmlib::BufferAllocator* allocator) {
  oscillator_.Init();
  filter_.Init();
  filter_aux_.Init();

  shaper_.Init();
  shaper_right_.Init();
  input_dc_blocker_.Init();
  dc_blocker_[0].Init();
  dc_blocker_[1].Init();

  input_dc_blocker_.set_f<FREQUENCY_DIRTY>(0.00093f);
  dc_blocker_[0].set_f<FREQUENCY_DIRTY>(0.0001f);
  dc_blocker_[1].set_f<FREQUENCY_DIRTY>(0.0001f);
  Reset();
}

void Acid::Reset() {
  oscillator_.Init();
  filter_.Reset();
  filter_aux_.Reset();
  shaper_.Reset();
  shaper_right_.Reset();
  input_dc_blocker_.Reset();
  dc_blocker_[0].Reset();
  dc_blocker_[1].Reset();
  accent_ = 0.8f;      // what the voice hands us with LEVEL unpatched
  accent_sweep_ = 0.0f;
  prev_makeup_ = 1.0f;
  prev_drive_ = 1.0f;
  prev_main_scale_ = kMainScale;
}

void Acid::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  const bool stereo = false;
  const float block = static_cast<float>(size);

  const float f0 = NoteToFrequency(parameters.note);

  float waveshape, pw;
  MacroToWaveform(0.5f, &waveshape, &pw);

  if (f0 >= kOscillatorMaxFrequency) {
    pw = 0.5f;
  } else {
    CONSTRAIN(pw, f0 * 2.0f, 1.0f - 2.0f * f0);
  }

    oscillator_.Render(
        f0, pw, waveshape, aux, size);
  input_dc_blocker_.Process<FILTER_MODE_HIGH_PASS>(aux, size);

  if (parameters.trigger & TRIGGER_UNPATCHED) {
    accent_ = parameters.accent;
  } else if (parameters.trigger & TRIGGER_RISING_EDGE) {
    accent_ = parameters.accent;
  }

  if (parameters.trigger & TRIGGER_HIGH) {
    const float charge = (accent_ - accent_sweep_) *
        (block / (kAccentSweepChargeTime * kSampleRate));
    if (charge > 0.0f) {
      accent_sweep_ += charge;
    }
  }
  accent_sweep_ -= accent_sweep_ * (block / (kAccentSweepTime * kSampleRate));

  float cutoff = kMinCutoff + parameters.morph * kCutoffRange
      + kKeyboardTracking * (parameters.note - kTrackingCenter)
      + kAccentBrightness * accent_ * accent_
      + kAccentSweepBrightness * accent_sweep_;
  CONSTRAIN(cutoff, 8.0f, 128.0f);

  float fc = NoteToFrequency(
      stereo ? cutoff - 0.5f * kStereoCutoffOffset : cutoff);
  CONSTRAIN(fc, 0.0005f, kMaxCutoffFrequency);
  filter_.set_f(fc);

  float resonance = parameters.harmonics;
  CONSTRAIN(resonance, 0.0f, 1.0f);

  const float k = SkewResonance(resonance) * kMaxResonance;

    filter_.set_resonance(k);

  const float makeup = 1.0f + kBassMakeup * k;

  const float amount = parameters.timbre * (1.0f + 0.5f * accent_);
  const float drive = 1.0f + kDriveGain * amount * amount;

  const float main_scale = MainScale(pw, amount);

  ParameterInterpolator makeup_mod(&prev_makeup_, makeup, size);
  ParameterInterpolator drive_mod(&prev_drive_, drive, size);
  ParameterInterpolator main_scale_mod(&prev_main_scale_, main_scale, size);

  for (size_t i = 0; i < size; ++i) {
#if PLAITS_ACID_INVERT_OSCILLATOR
    const float oscillator = -aux[i];
#else
    const float oscillator = aux[i];
#endif
    const float makeup_gain = makeup_mod.Next();
    const float drive_gain = drive_mod.Next();
    const float scale = main_scale_mod.Next();

    const float filtered = filter_.Process(oscillator) * makeup_gain;
    const float driven =
        shaper_.Process(filtered * kPreDriveScale * drive_gain) * scale;

      out[i] = driven;
      aux[i] = SoftClip(filtered * kAuxSaturation) * kAuxScale;

  }

  dc_blocker_[0].Process<FILTER_MODE_HIGH_PASS>(out, size);
  dc_blocker_[1].Process<FILTER_MODE_HIGH_PASS>(aux, size);
}

}  // namespace plaits
