// Copyright 2026 Lyle Mills.
// SPDX-License-Identifier: MIT
//
// Phase-cancellation oscillator bank.
//
// From Plaits Palette, ported to Mutable Instruments' engine interface: mono,
// no FM input, and Palette's fourth macro fixed at its neutral 0.5.
//
// OUT = weighted sum of four phase-offset harmonic-wave voices.
// AUX = orthogonal pair-difference field emphasizing the rejected partials.

#include "beads/blob/models/phase_weave_engine.h"

#include <algorithm>
#include <cmath>

#include "plaits/dsp/oscillator/sine_oscillator.h"
#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

const int kPhaseWeavePartials = 8;

// Palette's fourth macro at its neutral value.
const float kPhaseWeaveMacro = 0.5f;

inline float WrapPhase(float phase) {
  while (phase < 0.0f) {
    phase += 1.0f;
  }
  while (phase >= 1.0f) {
    phase -= 1.0f;
  }
  return phase;
}

// A band-limited harmonic wave: the Chebyshev recurrence needs one sine
// lookup per voice, not one per partial.
inline float HarmonicWave(
    float phase,
    const float* partial_amplitude) {
  const float x = SineNoWrap(WrapPhase(phase));
  const float two_x = 2.0f * x;
  float previous = 1.0f;
  float current = x;
  float sum = 0.0f;
  for (int i = 0; i < kPhaseWeavePartials; ++i) {
    sum += partial_amplitude[i] * current;
    const float next = two_x * current - previous;
    previous = current;
    current = next;
  }
  return sum;
}

}  // namespace

void PhaseWeave::Init(BufferAllocator* allocator) {
  Reset();
}

void PhaseWeave::Reset() {
  phase_ = 0.0f;
  constellation_ = 0.5f;
  spread_ = 0.0f;
  cancellation_ = 0.0f;
  rotation_ = 0.0f;
}

void PhaseWeave::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    phase_ = 0.0f;
  }

  const float frequency = min(0.24f, NoteToFrequency(parameters.note));

  // HARMONICS also tilts the even/odd balance, audible at minimum spread.
  float partial_amplitude[kPhaseWeavePartials];
  float partial_sum = 0.0f;
  for (int i = 0; i < kPhaseWeavePartials; ++i) {
    const int harmonic = i + 1;
    const float anti_alias = max(
        0.0f, 1.0f - 2.0f * frequency * static_cast<float>(harmonic));
    const float parity = harmonic & 1
        ? 1.0f - 0.22f * parameters.harmonics
        : 0.35f + 0.65f * parameters.harmonics;
    partial_amplitude[i] = anti_alias * parity / \
        static_cast<float>(harmonic);
    partial_sum += fabsf(partial_amplitude[i]);
  }
  const float partial_gain = partial_sum > 0.0f
      ? 1.0f / partial_sum
      : 0.0f;
  for (int i = 0; i < kPhaseWeavePartials; ++i) {
    partial_amplitude[i] *= partial_gain;
  }

  const float target_spread = 0.48f * parameters.timbre * parameters.timbre;
  const float target_rotation = (kPhaseWeaveMacro - 0.5f) * 0.36f;
  ParameterInterpolator constellation_modulation(
      &constellation_, parameters.harmonics, size);
  ParameterInterpolator spread_modulation(&spread_, target_spread, size);
  ParameterInterpolator cancellation_modulation(
      &cancellation_, parameters.morph, size);
  ParameterInterpolator rotation_modulation(
      &rotation_, target_rotation, size);

  for (size_t i = 0; i < size; ++i) {
    const float constellation = constellation_modulation.Next();
    const float spread = spread_modulation.Next();
    const float cancellation = cancellation_modulation.Next();
    const float rotation = rotation_modulation.Next();

    float f = frequency;
    CONSTRAIN(f, -0.24f, 0.24f);
    phase_ += f;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
    } else if (phase_ < 0.0f) {
      phase_ += 1.0f;
    }

    // Uneven offsets avoid one dominant PWM null; HARMONICS moves them from
    // clustered through skewed to near quadrature.
    float offset[4];
    offset[0] = 0.0f;
    offset[1] = spread * (0.28f + 0.28f * constellation) + rotation;
    offset[2] = spread * (-0.64f + 0.20f * constellation) - \
        0.63f * rotation;
    offset[3] = spread * (0.86f - 0.32f * constellation) + \
        0.37f * rotation;

    float voice[4];
    for (int j = 0; j < 4; ++j) {
      voice[j] = HarmonicWave(phase_ + offset[j], partial_amplitude);
    }

    // MORPH flips two lanes through zero to cancel; the unequal other two
    // keep sound at the deepest nulls.
    const float bipolar = 1.0f - 2.0f * cancellation;
    const float weight[4] = {
      1.0f,
      bipolar,
      0.72f,
      0.55f * bipolar
    };
    const float weight_sum = fabsf(weight[0]) + fabsf(weight[1]) + \
        fabsf(weight[2]) + fabsf(weight[3]);

    // Off the macro midpoint: rotation and drive; SoftClip bounds the peaks.
    const float drive = 1.0f + 11.111111f * fabsf(rotation);

    float main = 0.0f;
    for (int j = 0; j < 4; ++j) {
      main += voice[j] * weight[j];
    }
    main *= 0.92f / weight_sum;

    // AUX, the pair differences, carries what OUT cancels at any MORPH.
    const float difference = \
        0.58f * (voice[0] - voice[2]) + \
        0.42f * (voice[1] - voice[3]);

    out[i] = 0.9f * SoftClip(main * drive);
    aux[i] = 0.72f * SoftClip(difference * (0.8f + 0.2f * drive));
  }
}

}  // namespace plaits
