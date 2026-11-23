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
// CircuitZaps: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/circuit_zaps_engine.h"

#include <algorithm>

#include "plaits/dsp/oscillator/sine_oscillator.h"
#include "stmlib/dsp/dsp.h"
#include "stmlib/utils/random.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

inline float Noise() {
  return 2.0f * Random::GetFloat() - 1.0f;
}

inline float DecayCoefficient(float seconds) {
  return 1.0f - 1.0f / max(2.0f, seconds * kSampleRate);
}

inline float Triangle(float phase) {
  const float ramp = phase < 0.5f ? phase : 1.0f - phase;
  return 4.0f * ramp - 1.0f;
}

inline float SoftClip(float value, float drive) {
  const float x = value * drive;
  return x / (1.0f + fabsf(x));
}

}  // namespace

void CircuitZaps::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void CircuitZaps::Reset() {
  phase_a_ = 0.0f;
  phase_b_ = 0.0f;
  body_envelope_ = 0.0f;
  sweep_ratio_ = 1.0f;
  spark_envelope_ = 0.0f;
  spark_lowpass_ = 0.0f;
  spark_lowpass_aux_ = 0.0f;
  out_dc_ = 0.0f;
  aux_dc_ = 0.0f;
}

void CircuitZaps::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = true;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    const float accent = 0.30f + 0.70f * parameters.accent;
    body_envelope_ = accent;
    // The minimum sweep falls by a fifth.
    const float sweep_depth = 7.0f + 41.0f * parameters.timbre *
        parameters.timbre;
    sweep_ratio_ = SemitonesToRatio(sweep_depth);
    spark_envelope_ = accent;
    phase_a_ = 0.0f;
    phase_b_ = 0.0f;
  }

  const float base = min(0.12f, NoteToFrequency(parameters.note));
  const float topology = parameters.harmonics;
  const float ratio = 1.35f + 2.15f * topology * topology;
  const float decay_time = 0.025f + 1.20f * parameters.morph *
      parameters.morph;
  const float charge = 0.5f * 0.5f;
  const float body_decay = DecayCoefficient(decay_time);
  const float sweep_decay = DecayCoefficient(
      0.004f + 0.110f * parameters.timbre);
  const float spark_decay = DecayCoefficient(0.0008f + 0.026f * charge);
  const float drive = 1.2f + 7.0f * charge;

  for (size_t i = 0; i < size; ++i) {
    float frequency_a = min(0.235f, base * sweep_ratio_);
    float frequency_b = min(0.235f, frequency_a * ratio);
    phase_a_ += frequency_a;
    phase_a_ -= static_cast<int>(phase_a_);
    phase_b_ += frequency_b;
    phase_b_ -= static_cast<int>(phase_b_);

    const float sine = SineNoWrap(phase_a_);
    const float partner = 0.68f * SineNoWrap(phase_b_) +
        0.32f * Triangle(phase_b_);
    const float dual = 0.72f * sine + 0.42f * partner;
    const float ring = sine * partner;
    const float axis = topology * 2.0f;
    const float voice = axis < 1.0f
        ? sine + (dual - sine) * axis
        : dual + (ring - dual) * (axis - 1.0f);

    const float noise_left = Noise();
    spark_lowpass_ += 0.12f * (noise_left - spark_lowpass_);
    const float spark_left =
        (noise_left - spark_lowpass_) * spark_envelope_;
    const float clean = voice * body_envelope_;
    const float driven_input = clean + 0.68f * charge * spark_left;
    const float saturated = 1.45f * SoftClip(driven_input, drive);
    const float dirty =
        driven_input + charge * (saturated - driven_input);
    const float aux_sample = 0.72f * clean + 0.28f * spark_left;
    out_dc_ += 0.001f * (dirty - out_dc_);
    aux_dc_ += 0.001f * (aux_sample - aux_dc_);
    // Trim the saturated signal to retain headroom.
    out[i] = 0.62f * (dirty - out_dc_);
    aux[i] = aux_sample - aux_dc_;


    body_envelope_ *= body_decay;
    sweep_ratio_ += (1.0f - sweep_decay) * (1.0f - sweep_ratio_);
    spark_envelope_ *= spark_decay;
  }
}

}  // namespace plaits
