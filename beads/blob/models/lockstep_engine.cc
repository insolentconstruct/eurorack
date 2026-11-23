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
// Lockstep: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/lockstep_engine.h"

#include <algorithm>
#include <cmath>

#include "plaits/dsp/oscillator/sine_oscillator.h"
#include "stmlib/dsp/units.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

struct LockRatio {
  int numerator;
  int denominator;
};

const LockRatio kLockRatios[] = {
  { 1, 4 }, { 1, 3 }, { 1, 2 }, { 2, 3 }, { 3, 4 },
  { 1, 1 }, { 4, 3 }, { 3, 2 }, { 2, 1 }, { 5, 2 },
  { 3, 1 }, { 4, 1 }, { 5, 1 }, { 6, 1 }, { 8, 1 }
};

const int kNumLockRatios = sizeof(kLockRatios) / sizeof(kLockRatios[0]);

inline float Wrap(float phase) {
  phase -= static_cast<int>(phase);
  if (phase < 0.0f) {
    phase += 1.0f;
  }
  return phase;
}

inline float WrapBipolar(float phase) {
  phase = Wrap(phase + 0.5f);
  return phase - 0.5f;
}

inline float LimitAudio(float sample) {
  CONSTRAIN(sample, -1.0f, 1.0f);
  return sample;
}

}  // namespace

void Lockstep::Init(BufferAllocator* allocator) {
  Reset();
}

void Lockstep::Reset() {
  reference_phase_ = 0.0f;
  follower_phase_ = 0.0f;
  follower_frequency_ = 0.0f;
  detector_lp_ = 0.0f;
  reset_pending_ = true;
}

void Lockstep::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  int ratio_index = static_cast<int>(
      parameters.harmonics * static_cast<float>(kNumLockRatios));
  if (ratio_index >= kNumLockRatios) {
    ratio_index = kNumLockRatios - 1;
  }
  const LockRatio& lock_ratio = kLockRatios[ratio_index];
  const float numerator = static_cast<float>(lock_ratio.numerator);
  const float denominator = static_cast<float>(lock_ratio.denominator);
  const float ratio = numerator / denominator;

  const float reference_ceiling = 0.22f / max(1.0f, ratio);
  const float base_reference_frequency = min(
      NoteToFrequency(parameters.note), reference_ceiling);
  const float base_target_frequency = base_reference_frequency * ratio;

  if (reset_pending_) {
    follower_frequency_ = base_target_frequency * 0.82f;
    reset_pending_ = false;
  }
  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    follower_phase_ = Wrap(
        follower_phase_ + 0.29f + 0.17f * parameters.accent);
    follower_frequency_ *= 0.70f + 0.16f * parameters.morph;
    detector_lp_ = 0.0f;
  }

  const float bandwidth = 0.00012f + 0.0075f * parameters.timbre *
      parameters.timbre;
  const float damping = 0.35f + 1.45f * 0.5f;
  const float proportional_gain = min(0.08f, 2.0f * damping * bandwidth);
  const float integral_gain = bandwidth * bandwidth;

  const float feedthrough_index = kLockstepFeedthrough * 0.5f;
  const float capture_octaves = 0.10f + 1.85f * 0.5f;
  const float capture_min_ratio = SemitonesToRatio(-12.0f * capture_octaves);
  const float capture_max_ratio = SemitonesToRatio(12.0f * capture_octaves);
  const float detector_smoothing = 0.002f +
      0.075f * parameters.timbre * parameters.timbre;

  for (size_t i = 0; i < size; ++i) {
    float reference_frequency = base_reference_frequency;
    const float target_frequency = reference_frequency * ratio;
    float capture_min = target_frequency * capture_min_ratio;
    float capture_max = target_frequency * capture_max_ratio;
    reference_phase_ = Wrap(reference_phase_ + reference_frequency);

    const float reference_compare = Wrap(reference_phase_ * numerator);
    const float follower_compare = Wrap(follower_phase_ * denominator);
    const float phase_error = WrapBipolar(
        reference_compare - follower_compare);
    const float sine_detector = Sine(8.0f + phase_error);
    const float saw_detector = phase_error * 2.0f;
    const float detector = sine_detector +
        (saw_detector - sine_detector) * parameters.morph;

    follower_frequency_ += integral_gain * detector;
    follower_frequency_ += bandwidth * 0.0015f *
        (target_frequency - follower_frequency_);
    CONSTRAIN(follower_frequency_, capture_min, capture_max);

    follower_phase_ = Wrap(
        follower_phase_ + follower_frequency_ +
        proportional_gain * detector);

    const float deadband = 0.012f + 0.055f * (1.0f - parameters.morph);
    const float charge = phase_error > deadband
        ? 1.0f
        : (phase_error < -deadband ? -1.0f : 0.0f);
    detector_lp_ += detector_smoothing * (charge - detector_lp_);

    const float ripple = SineNoWrap(reference_compare);
    const float carrier = SineNoWrap(
        Wrap(follower_phase_ + feedthrough_index * ripple));
    const float soft_square = carrier /
        (0.22f + 0.78f * fabsf(carrier));
    const float error_shape = min(
        1.0f, parameters.morph + 4.0f * fabsf(phase_error));
    const float out_signal =
        (carrier + (soft_square - carrier) * error_shape) * 0.78f;

    out[i] = out_signal;
    const float reference = SineNoWrap(reference_phase_);
    aux[i] = LimitAudio(
        detector_lp_ * 0.58f + (reference - carrier) * 0.31f);
  }
}

}  // namespace plaits
