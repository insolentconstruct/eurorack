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
// Glisson synthesis: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/glisson_engine.h"

#include <algorithm>

#include "plaits/dsp/oscillator/sine_oscillator.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

// Keep chirps below the least useful top octave. This intentionally uses a
// cheap clamp: the engine calls it twice per grain per sample on a 72 MHz MCU.
float LimitFrequency(float frequency) {
  return min(0.22f, frequency);
}

}  // namespace

void Glisson::Init(BufferAllocator* allocator) {
  Reset();
}

void Glisson::Reset() {
  for (int i = 0; i < kNumGlissonGrains; ++i) {
    grain_[i].phase = 0.0f;
    grain_[i].aux_phase = 0.0f;
    grain_[i].envelope_phase = 1.0f;
    grain_[i].envelope_increment = 0.001f;
    grain_[i].start_ratio = 1.0f;
    grain_[i].end_ratio = 1.0f;
  }
  num_grains_ = 0;
  reset_pending_ = true;
}

void Glisson::StartGrain(
    Grain* grain,
    float scatter,
    float direction,
    float duration,
    float delay) {
  const float random_pitch = 2.0f * Random::GetFloat() - 1.0f;
  const float centre = random_pitch * scatter;
  const float excursion = direction * (3.0f + 0.65f * scatter);

  grain->start_ratio = SemitonesToRatio(centre - 0.5f * excursion);
  grain->end_ratio = SemitonesToRatio(centre + 0.5f * excursion);
  grain->envelope_phase = -delay;
  grain->envelope_increment = 1.0f / duration;
}

// Keep grain state local so output stores cannot alias its phases.
template<bool render_aux>
void Glisson::RenderMono(
    const EngineParameters& parameters,
    int num_grains,
    float f0,
    float scatter,
    float direction,
    float duration,
    float gain,
    float* out,
    float* aux,
    size_t size) {
  const float macro = 0.5f;
  for (int i = 0; i < num_grains; ++i) {
    Grain grain = grain_[i];
    Grain* g = &grain;
    for (size_t j = 0; j < size; ++j) {
      g->envelope_phase += g->envelope_increment;
      if (g->envelope_phase >= 1.0f) {
        StartGrain(g, scatter, direction, duration, 0.0f);
      }
      if (g->envelope_phase < 0.0f) {
        continue;
      }

      float t = g->envelope_phase;
      // The fourth macro also bends the trajectory: short grains are nearly
      // linear, long grains linger at their endpoints.
      const float curved = t * t * (3.0f - 2.0f * t);
      t += (curved - t) * macro;
      const float ratio = g->start_ratio + \
          (g->end_ratio - g->start_ratio) * t;
      const float reverse_ratio = render_aux ? g->end_ratio + \
          (g->start_ratio - g->end_ratio) * t : 0.0f;

      float fundamental = f0;
      g->phase += LimitFrequency(fundamental * ratio);
      g->phase -= static_cast<int>(g->phase);
      if (render_aux) {
        g->aux_phase += LimitFrequency(fundamental * reverse_ratio);
        g->aux_phase -= static_cast<int>(g->aux_phase);
      }

      // The parabola saves a sine lookup per grain sample. The oscillator
      // phases are already wrapped for SineNoWrap.
      const float envelope = 4.0f * g->envelope_phase * \
          (1.0f - g->envelope_phase);
      out[j] += SineNoWrap(g->phase) * envelope * gain;
      if (render_aux) {
        aux[j] += SineNoWrap(g->aux_phase) * envelope * gain;
      }
    }
    grain_[i] = grain;
  }
}

void Glisson::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  fill(&out[0], &out[size], 0.0f);
  fill(&aux[0], &aux[size], 0.0f);

  const float f0 = NoteToFrequency(parameters.note);
  const float scatter = 2.0f + 34.0f * parameters.harmonics * \
      parameters.harmonics;
  const float direction = 2.0f * parameters.morph - 1.0f;
  const float duration_seconds = 0.012f * SemitonesToRatio(
      0.5f * 60.0f);
  const float duration = duration_seconds * kCorrectedSampleRate;
  const int num_grains = 1 + static_cast<int>(
      parameters.timbre * static_cast<float>(kNumGlissonGrains - 1) + 0.5f);

  if (reset_pending_ || (parameters.trigger & TRIGGER_RISING_EDGE)) {
    for (int i = 0; i < kNumGlissonGrains; ++i) {
      StartGrain(
          &grain_[i],
          scatter,
          direction,
          duration,
          static_cast<float>(i) / static_cast<float>(num_grains));
    }
    reset_pending_ = false;
  } else if (num_grains > num_grains_) {
    for (int i = num_grains_; i < num_grains; ++i) {
      StartGrain(
          &grain_[i], scatter, direction, duration, 0.0f);
    }
  }
  num_grains_ = num_grains;

  const float gain = 0.72f / static_cast<float>(num_grains);
  RenderMono<true>(parameters, num_grains, f0, scatter, direction,
      duration, gain, out, aux, size);
}

}  // namespace plaits
