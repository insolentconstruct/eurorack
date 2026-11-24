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
// Granular Cloud: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/granular_cloud_engine.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/units.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

const float kBraidsSinePhaseOffset = 0.75f;

const float kBraidsSineAmplitude = 32638.0f / 32768.0f;
const float kBraidsSineDc = 127.0f / 32768.0f;

inline float BraidsSine(float phase) {
  phase += kBraidsSinePhaseOffset;
  if (phase >= 1.0f) {
    phase -= 1.0f;
  }
  return Sine(phase) * kBraidsSineAmplitude + kBraidsSineDc;
}

inline float GrainEnvelope(float position, float peak, float rise, float fall) {
  if (position >= 1.0f) {
    return 0.0f;
  }
  const float warped = position < peak
      ? position * rise
      : 0.5f + (position - peak) * fall;
  return 0.5f - 0.5f * Sine(warped + 0.25f);
}

}  // namespace

inline uint32_t GranularCloud::NextWord() {
  rng_state_ = rng_state_ * 1664525L + 1013904223L;
  return rng_state_;
}

void GranularCloud::Init(BufferAllocator* allocator) {
  (void) allocator;
  // Reset retains this stream, as in Palette.
  rng_state_ = kGranularCloudSeed;
  Reset();
}

void GranularCloud::Reset() {
  for (int i = 0; i < kGranularCloudNumGrains; ++i) {
    grain_[i].phase = 0.0f;
    grain_[i].phase_increment = 0.0f;
    grain_[i].envelope_phase = 0;
    grain_[i].envelope_phase_increment = 0;
  }
  aux_phase_ = 0.0f;
  peak_position_ = 0.5f;
  scheduler_countdown_ = 0;
}

void GranularCloud::ScheduleGrains(
    int timbre_code,
    int color_code,
    uint32_t spawn_threshold,
    float base_increment) {
  for (int i = 0; i < kGranularCloudNumGrains; ++i) {
    Grain* g = &grain_[i];
    if (g->envelope_phase <= kGranularCloudEnvelopeEnd &&
        g->envelope_phase_increment != 0) {
      continue;
    }
    g->envelope_phase_increment = 0;
    if ((NextWord() & 0xffff) >= spawn_threshold) {
      continue;
    }

    const int index = timbre_code >> 7;
    const uint32_t rate = static_cast<uint32_t>(
        kGranularCloudRateBase * SemitonesToRatio(
            static_cast<float>(index) * kGranularCloudRateSemitonesPerStep));
    g->envelope_phase_increment = rate << kGranularCloudRateShift;
    g->envelope_phase = 0;
    g->phase_increment = base_increment;

    const int32_t sample = static_cast<int32_t>(
        static_cast<int16_t>(NextWord() >> 16));
    const int32_t pitch_mod = (sample * color_code) >> 16;
    const int32_t steps = pitch_mod < 0 ? (pitch_mod >> 8) : (pitch_mod >> 7);
    g->phase_increment += base_increment *
        static_cast<float>(steps) * (1.0f / 256.0f);
    CONSTRAIN(g->phase_increment, 0.0f, 0.5f);
  }
}

void GranularCloud::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  const float base_increment = NoteToFrequency(parameters.note);

  float timbre = parameters.timbre;
  float color = parameters.harmonics;
  CONSTRAIN(timbre, 0.0f, 1.0f);
  CONSTRAIN(color, 0.0f, 1.0f);
  const int timbre_code = static_cast<int>(timbre * 32767.0f);
  const int color_code = static_cast<int>(color * 32767.0f);

  const float spawn = kGranularCloudSpawnStock;
  const uint32_t spawn_threshold = static_cast<uint32_t>(spawn);

  float morph = parameters.morph;
  CONSTRAIN(morph, 0.0f, 1.0f);
  const float target_peak = kGranularCloudPeakLate +
      (kGranularCloudPeakEarly - kGranularCloudPeakLate) * morph;

  const bool peak_moving = target_peak != peak_position_;
  float rise = 0.5f / target_peak;
  float fall = 0.5f / (1.0f - target_peak);
  ParameterInterpolator peak_modulation(&peak_position_, target_peak, size);

  const float inverse_end = 1.0f / static_cast<float>(kGranularCloudEnvelopeEnd);

  while (size--) {
    const float pitch_ratio = 1.0f;
    if (scheduler_countdown_ <= 0) {
      ScheduleGrains(timbre_code, color_code, spawn_threshold, base_increment);
      scheduler_countdown_ = kGranularCloudSchedulerPeriod;
    }
    --scheduler_countdown_;

    const float peak = peak_modulation.Next();
    if (peak_moving) {
      rise = 0.5f / peak;
      fall = 0.5f / (1.0f - peak);
    }

    float mono = 0.0f;
    float envelope_sum = 0.0f;

    for (int i = 0; i < kGranularCloudNumGrains; ++i) {
      Grain* g = &grain_[i];
      g->phase += g->phase_increment * pitch_ratio;
      if (g->phase >= 1.0f) {
        g->phase -= 1.0f;
      }
      g->envelope_phase += g->envelope_phase_increment;

      const float envelope = GrainEnvelope(
          static_cast<float>(g->envelope_phase) * inverse_end, peak, rise, fall);
      const float value = BraidsSine(g->phase) * envelope *
          kGranularCloudGrainGain;

      mono += value;
      envelope_sum += envelope * kGranularCloudGrainGain;

    }

    CONSTRAIN(mono, -1.0f, 1.0f);

    aux_phase_ += base_increment * pitch_ratio;
    if (aux_phase_ >= 1.0f) {
      aux_phase_ -= 1.0f;
    }

    *out++ = mono;
    *aux++ = BraidsSine(aux_phase_) * envelope_sum;

  }
}

}  // namespace plaits
