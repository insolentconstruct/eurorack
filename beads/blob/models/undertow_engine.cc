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
// Undertow: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/undertow_engine.h"

#include <algorithm>
#include <cmath>

#include "plaits/dsp/dsp.h"
#include "stmlib/dsp/polyblep.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

const int kNumUndertowRegistrations = 5;

// Columns are the pitch anchor followed by divisors 2 through 6.
const float kUndertowRegistration[kNumUndertowRegistrations]
    [kNumUndertowVoices] = {
  { 0.80f, 0.70f, 0.00f, 0.22f, 0.00f, 0.00f },
  { 0.42f, 0.82f, 0.08f, 0.58f, 0.00f, 0.34f },
  { 0.34f, 0.12f, 0.82f, 0.08f, 0.66f, 0.00f },
  { 0.30f, 0.72f, 0.58f, 0.48f, 0.38f, 0.28f },
  { 0.28f, 0.24f, 0.34f, 0.62f, 0.54f, 0.86f }
};

inline float WrapPhase(float phase) {
  if (phase >= 1.0f) {
    phase -= 1.0f;
  }
  return phase;
}

// Two-point polynomial BLEP for the saw and pulse discontinuities.
inline float PolyBlep(float phase, float frequency) {
  if (phase < frequency) {
    const float t = phase / frequency;
    return t + t - t * t - 1.0f;
  }
  if (phase > 1.0f - frequency) {
    const float t = (phase - 1.0f) / frequency;
    return t * t + t + t + 1.0f;
  }
  return 0.0f;
}

inline float TriangleSawWave(
    float phase,
    float frequency,
    float mix) {
  const float triangle = 1.0f - 4.0f * fabsf(phase - 0.5f);
  const float saw = 2.0f * phase - 1.0f - PolyBlep(phase, frequency);
  return triangle + (saw - triangle) * mix;
}

inline float BandlimitedSawPulseWave(
    float phase,
    float frequency,
    float mix,
    float pulse_width,
    float previous_pulse_width,
    bool wrapped,
    bool* pulse_high,
    float* next_blep,
    bool signed_fm) {
  const float saw = 2.0f * phase - 1.0f;
  float this_blep = *next_blep;
  *next_blep = 0.0f;

  {
    if (wrapped) {
      const float t = phase / frequency;
      // Saw falls by 2 at wrap while pulse rises by 2.
      const float discontinuity = -2.0f + 4.0f * mix;
      this_blep += discontinuity * ThisBlepSample(t);
      *next_blep += discontinuity * NextBlepSample(t);
      *pulse_high = true;
    }

    if (*pulse_high && phase >= pulse_width) {
      const float denominator =
          previous_pulse_width - pulse_width + frequency;
      const float t = (phase - pulse_width) / denominator;
      const float discontinuity = -2.0f * mix;
      this_blep += discontinuity * ThisBlepSample(t);
      *next_blep += discontinuity * NextBlepSample(t);
      *pulse_high = false;
    }
  }
  const float pulse = *pulse_high ? 1.0f : -1.0f;
  return saw + (pulse - saw) * mix + this_blep;
}

void RenderTriangleSawBlock(
    size_t begin,
    size_t end,
    float colour_start,
    float colour_increment,
    float* phase,
    float* frequency,
    const float* frequency_increment,
    const float* frequency_scale,
    const float* root_frequency_offset,
    float* main_amplitude,
    const float* main_increment,
    float* aux_amplitude,
    const float* aux_increment,
    float* out,
    float* aux) {
  float colour = colour_start + colour_increment * static_cast<float>(begin);
  for (size_t i = begin; i < end; ++i) {
    colour += colour_increment;
    const float wave_mix = colour * 2.0f;
    float out_sum = 0.0f;
    float aux_sum = 0.0f;
    for (int voice = 0; voice < kNumUndertowVoices; ++voice) {
      frequency[voice] += frequency_increment[voice];
      const float instantaneous_frequency = frequency[voice];
      main_amplitude[voice] += main_increment[voice];
      aux_amplitude[voice] += aux_increment[voice];
      phase[voice] = WrapPhase(phase[voice] + instantaneous_frequency);
      const float sample = TriangleSawWave(
          phase[voice], instantaneous_frequency, wave_mix);
      out_sum += sample * main_amplitude[voice];
      aux_sum += sample * aux_amplitude[voice];
    }
    out[i] = out_sum;
    aux[i] = aux_sum;
  }
}

void RenderSawPulseBlock(
    size_t begin,
    size_t end,
    float colour_start,
    float colour_increment,
    bool entering_pulse,
    float* phase,
    float* frequency,
    const float* frequency_increment,
    const float* frequency_scale,
    const float* root_frequency_offset,
    float* main_amplitude,
    const float* main_increment,
    float* aux_amplitude,
    const float* aux_increment,
    float* pulse_width_state,
    bool* pulse_high,
    float* next_blep,
    float* out,
    float* aux) {
  float colour = colour_start + colour_increment * static_cast<float>(begin);
  for (size_t i = begin; i < end; ++i) {
    colour += colour_increment;
    const float wave_mix = colour * 2.0f - 1.0f;
    const float pulse_width = 0.5f - 0.36f * colour * colour;
    float out_sum = 0.0f;
    float aux_sum = 0.0f;
    // Correct the two brightest voices at their discontinuities.
    for (int voice = 0; voice < 2; ++voice) {
      frequency[voice] += frequency_increment[voice];
      const float instantaneous_frequency = frequency[voice];
      main_amplitude[voice] += main_increment[voice];
      aux_amplitude[voice] += aux_increment[voice];
      const float next_phase = phase[voice] + instantaneous_frequency;
      const bool wrapped = next_phase >= 1.0f;
      phase[voice] = WrapPhase(next_phase);
      // Only the lower anti-aliasing bound can become active.
      const float voice_pulse_width = max(
          pulse_width, 2.0f * instantaneous_frequency);
      if (entering_pulse && i == begin) {
        pulse_high[voice] = phase[voice] < voice_pulse_width;
        pulse_width_state[voice] = voice_pulse_width;
        next_blep[voice] = 0.0f;
      }
      const float sample = BandlimitedSawPulseWave(
          phase[voice],
          instantaneous_frequency,
          wave_mix,
          voice_pulse_width,
          pulse_width_state[voice],
          wrapped,
          &pulse_high[voice],
          &next_blep[voice], root_frequency_offset != NULL);
      pulse_width_state[voice] = voice_pulse_width;
      out_sum += sample * main_amplitude[voice];
      aux_sum += sample * aux_amplitude[voice];
    }

    for (int voice = 2; voice < kNumUndertowVoices; ++voice) {
      frequency[voice] += frequency_increment[voice];
      const float instantaneous_frequency = frequency[voice];
      main_amplitude[voice] += main_increment[voice];
      aux_amplitude[voice] += aux_increment[voice];
      phase[voice] = WrapPhase(phase[voice] + instantaneous_frequency);
      const float voice_pulse_width = max(
          pulse_width, 2.0f * instantaneous_frequency);
      const float saw = 2.0f * phase[voice] - 1.0f;
      const float pulse = phase[voice] < voice_pulse_width ? 1.0f : -1.0f;
      const float sample = saw + (pulse - saw) * wave_mix;
      out_sum += sample * main_amplitude[voice];
      aux_sum += sample * aux_amplitude[voice];
    }
    out[i] = out_sum;
    aux[i] = aux_sum;
  }
}

}  // namespace

void Undertow::Init(BufferAllocator* allocator) {
  Reset();
}

void Undertow::Reset() {
  for (int i = 0; i < kNumUndertowVoices; ++i) {
    phase_[i] = 0.0f;
    frequency_[i] = 0.001f;
    main_amplitude_[i] = 0.0f;
    aux_amplitude_[i] = 0.0f;
    next_blep_[i] = 0.0f;
    pulse_high_[i] = true;
    pulse_width_[i] = 0.41f;
  }
  colour_ = 0.5f;
  pulse_mode_ = false;
}

void Undertow::ComputeRegistration(
    float registration,
    float* amplitude) const {
  registration *= static_cast<float>(kNumUndertowRegistrations - 1);
  int integral = static_cast<int>(registration);
  if (integral >= kNumUndertowRegistrations - 1) {
    integral = kNumUndertowRegistrations - 2;
    registration = static_cast<float>(kNumUndertowRegistrations - 1);
  }
  const float fractional = registration - static_cast<float>(integral);
  for (int i = 0; i < kNumUndertowVoices; ++i) {
    const float a = kUndertowRegistration[integral][i];
    const float b = kUndertowRegistration[integral + 1][i];
    amplitude[i] = a + (b - a) * fractional;
  }
}

void Undertow::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    // Triggered notes start every divider at the same crossing.
    fill(&phase_[0], &phase_[kNumUndertowVoices], 0.0f);
    fill(&next_blep_[0], &next_blep_[kNumUndertowVoices], 0.0f);
    fill(&pulse_high_[0], &pulse_high_[kNumUndertowVoices], true);
    pulse_mode_ = false;
  }

  float registration[kNumUndertowVoices];
  ComputeRegistration(parameters.harmonics, registration);

  float main_target[kNumUndertowVoices];
  float aux_target[kNumUndertowVoices];
  float main_sum = 0.0f;
  float aux_sum = 0.0f;
  for (int i = 0; i < kNumUndertowVoices; ++i) {
    const float balance = i == 0
        ? 1.45f - 0.65f * parameters.morph
        : 0.32f + 1.08f * parameters.morph;
    main_target[i] = registration[i] * balance;
    main_sum += fabsf(main_target[i]);

    // AUX removes the anchor and alternates the divider polarities.
    const float polarity = (i & 1) ? 1.0f : -1.0f;
    aux_target[i] = i == 0 ? 0.0f : registration[i] * polarity;
    aux_sum += fabsf(aux_target[i]);
  }
  const float main_gain = main_sum > 0.0f ? 0.86f / main_sum : 0.0f;
  const float aux_gain = aux_sum > 0.0f ? 0.76f / aux_sum : 0.0f;

  const float anchor_frequency = min(
      0.24f, max(0.000001f, NoteToFrequency(parameters.note)));
  // The fixed macro midpoint gives an exact integer undertone lattice.
  const float lattice_stretch = (0.5f - 0.5f) * 0.18f;
  const float colour_start = colour_;
  const float colour_increment =
      (parameters.timbre - colour_) / static_cast<float>(size);

  float frequency[kNumUndertowVoices];
  float frequency_increment[kNumUndertowVoices];
  float frequency_scale[kNumUndertowVoices];
  float main_amplitude[kNumUndertowVoices];
  float main_increment[kNumUndertowVoices];
  float aux_amplitude[kNumUndertowVoices];
  float aux_increment[kNumUndertowVoices];
  for (int voice = 0; voice < kNumUndertowVoices; ++voice) {
    const float divisor = static_cast<float>(voice + 1);
    const float distance = divisor - 1.0f;
    const float warped_divisor = divisor + lattice_stretch *
        distance * distance / static_cast<float>(kNumUndertowVoices - 1);
    const float target_frequency = anchor_frequency / warped_divisor;
    frequency_scale[voice] = 1.0f / warped_divisor;

    const float out_channel_target = main_target[voice] * main_gain;
    const float aux_channel_target = aux_target[voice] * aux_gain;
    frequency[voice] = frequency_[voice];
    frequency_increment[voice] =
        (target_frequency - frequency_[voice]) / static_cast<float>(size);
    main_amplitude[voice] = main_amplitude_[voice];
    main_increment[voice] =
        (out_channel_target - main_amplitude_[voice]) /
        static_cast<float>(size);
    aux_amplitude[voice] = aux_amplitude_[voice];
    aux_increment[voice] =
        (aux_channel_target - aux_amplitude_[voice]) /
        static_cast<float>(size);
  }

  size_t boundary = 0;
  const bool starts_in_pulse = colour_start > 0.5f;
  const bool ends_in_pulse = parameters.timbre > 0.5f;
  if (starts_in_pulse == ends_in_pulse) {
    boundary = size;
  } else {
    while (boundary < size) {
      const float colour = colour_start +
          colour_increment * static_cast<float>(boundary + 1);
      if ((colour > 0.5f) != starts_in_pulse) {
        break;
      }
      ++boundary;
    }
  }
  if (starts_in_pulse) {
    if (boundary > 0) {
      RenderSawPulseBlock(
          0, boundary, colour_start, colour_increment, !pulse_mode_,
          phase_, frequency, frequency_increment, frequency_scale,
          NULL,
          main_amplitude, main_increment, aux_amplitude, aux_increment,
          pulse_width_, pulse_high_, next_blep_, out, aux);
    }
    if (boundary < size) {
      RenderTriangleSawBlock(
          boundary, size, colour_start, colour_increment,
          phase_, frequency, frequency_increment, frequency_scale,
          NULL,
          main_amplitude, main_increment, aux_amplitude, aux_increment,
          out, aux);
      fill(&next_blep_[0], &next_blep_[kNumUndertowVoices], 0.0f);
    }
  } else {
    RenderTriangleSawBlock(
        0, boundary, colour_start, colour_increment,
        phase_, frequency, frequency_increment, frequency_scale,
        NULL,
        main_amplitude, main_increment, aux_amplitude, aux_increment,
        out, aux);
    fill(&next_blep_[0], &next_blep_[kNumUndertowVoices], 0.0f);
    if (boundary < size) {
      RenderSawPulseBlock(
          boundary, size, colour_start, colour_increment, true,
          phase_, frequency, frequency_increment, frequency_scale,
          NULL,
          main_amplitude, main_increment, aux_amplitude, aux_increment,
          pulse_width_, pulse_high_, next_blep_, out, aux);
    }
  }
  pulse_mode_ = parameters.timbre > 0.5f;

  for (int voice = 0; voice < kNumUndertowVoices; ++voice) {
    frequency_[voice] = frequency[voice];
    main_amplitude_[voice] = main_amplitude[voice];
    aux_amplitude_[voice] = aux_amplitude[voice];
  }
  colour_ = parameters.timbre;
}

}  // namespace plaits
