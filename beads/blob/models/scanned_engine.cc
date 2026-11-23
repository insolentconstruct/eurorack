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
// Scanned: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/scanned_engine.h"

#include <algorithm>
#include <cmath>

#include "plaits/dsp/oscillator/sine_oscillator.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"

namespace plaits {

using namespace std;
using namespace stmlib;

void Scanned::Init(BufferAllocator* allocator) {
  Reset();
}

void Scanned::Reset() {
  fill(&position_[0], &position_[kScannedMasses], 0.0f);
  fill(&velocity_[0], &velocity_[kScannedMasses], 0.0f);
  scan_phase_ = 0.0f;
  physics_phase_ = 0.0f;
  reset_pending_ = true;
  // No knob is ever negative, so the first Step() computes the masses.
  mass_inharmonicity_ = mass_structure_ = -1.0f;
}

void Scanned::Excite(float position, float width, float amount) {
  const float centre = position * static_cast<float>(kScannedMasses);
  const float half_inverse_width = 0.5f / width;

  // Strike wrapped windows in index order to preserve the mean sum.
  const int reach = static_cast<int>(width) + 1;
  const int first = static_cast<int>(
      centre + static_cast<float>(kScannedMasses)) - kScannedMasses - reach;
  const int end = first + 2 * reach + 2;
  int run_begin[2] = { 0, 0 };
  int run_end[2] = { kScannedMasses, 0 };
  if (end - first < kScannedMasses) {
    if (first < 0) {
      run_end[0] = end;
      run_begin[1] = first + kScannedMasses;
      run_end[1] = kScannedMasses;
    } else if (end > kScannedMasses) {
      run_end[0] = end - kScannedMasses;
      run_begin[1] = first;
      run_end[1] = kScannedMasses;
    } else {
      run_begin[0] = first;
      run_end[0] = end;
    }
  }

  float mean = 0.0f;
  for (int run = 0; run < 2; ++run) {
    for (int i = run_begin[run]; i < run_end[run]; ++i) {
      float distance = fabsf(static_cast<float>(i) - centre);
      distance = min(distance, static_cast<float>(kScannedMasses) - distance);
      if (distance < width) {
        const float pulse = 0.5f + 0.5f * Sine(
            0.25f + distance * half_inverse_width);
        velocity_[i] += pulse * amount;
        mean += pulse * amount;
      }
    }
  }
  mean /= static_cast<float>(kScannedMasses);
  for (int i = 0; i < kScannedMasses; ++i) {
    velocity_[i] -= mean;
  }
}

void Scanned::Step(
    float inharmonicity,
    float structure,
    float damping,
    float nonlinearity,
    bool driven,
    int drive_index) {
  if (inharmonicity != mass_inharmonicity_ || structure != mass_structure_) {
    for (int i = 0; i < kScannedMasses; ++i) {
      // Irregular masses make the excitation position change the spectrum.
      const float mass_profile = static_cast<float>((i * 13) & 31) / 31.0f;
      const float mass = 1.0f + inharmonicity * 0.45f * (i & 1) +
          structure * (0.25f + 1.35f * mass_profile);
      inverse_mass_[i] = 1.0f / mass;
    }
    mass_inharmonicity_ = inharmonicity;
    mass_structure_ = structure;
  }

  // Slide the five neighbours through registers, preserving arithmetic order.
  const float biharmonic_amount = 0.018f * inharmonicity;
  const float nonlinear_amount = nonlinearity * 0.12f;
  const int mask = kScannedMasses - 1;
  float l2 = position_[kScannedMasses - 2];
  float l1 = position_[kScannedMasses - 1];
  float here = position_[0];
  float r1 = position_[1];
  float acceleration[kScannedMasses];
  float acceleration_mean = 0.0f;
  for (int i = 0; i < kScannedMasses; ++i) {
    const float r2 = position_[(i + 2) & mask];
    const float laplacian = l1 + r1 - 2.0f * here;
    const float biharmonic = l2 - 4.0f * l1 + 6.0f * here - 4.0f * r1 + r2;
    float a = (0.22f * laplacian - biharmonic_amount * biharmonic) *
        inverse_mass_[i];
    a -= nonlinear_amount * here * fabsf(here);
    acceleration[i] = a;
    acceleration_mean += a;
    l2 = l1;
    l1 = here;
    here = r1;
    r1 = r2;
  }
  if (driven) {
    const float drive =
        (Random::GetFloat() - 0.5f) * (0.002f + 0.004f * nonlinearity);
    acceleration[drive_index] += drive;
    acceleration_mean += drive;
  }
  acceleration_mean /= static_cast<float>(kScannedMasses);

  float position_mean = 0.0f;
  float velocity_mean = 0.0f;
  for (int i = 0; i < kScannedMasses; ++i) {
    velocity_[i] = (velocity_[i] + acceleration[i] - acceleration_mean) *
        damping;
    position_[i] += velocity_[i];
    position_mean += position_[i];
    velocity_mean += velocity_[i];
  }
  position_mean /= static_cast<float>(kScannedMasses);
  velocity_mean /= static_cast<float>(kScannedMasses);

  float peak = 0.0f;
  for (int i = 0; i < kScannedMasses; ++i) {
    position_[i] -= position_mean;
    velocity_[i] -= velocity_mean;
    peak = max(peak, fabsf(position_[i]));
  }
  if (peak > 1.0f) {
    const float gain = 1.0f / peak;
    for (int i = 0; i < kScannedMasses; ++i) {
      position_[i] *= gain;
      velocity_[i] *= gain;
    }
  }
}

// Blend the scan with its derivative before sine wavefolding.
float Scanned::ReadPickup(
    float phase,
    float timbre,
    float fold_amount,
    float* derivative) const {
  // The pickup and derivative share an index and fractional position.
  const float index = phase * static_cast<float>(kScannedMasses);
  const int integral = static_cast<int>(index);
  const float fractional = index - static_cast<float>(integral);
  const int mask = kScannedMasses - 1;
  const float before = position_[(integral - 1) & mask];
  const float here = position_[integral];
  const float next = position_[(integral + 1) & mask];
  const float after = position_[(integral + 2) & mask];
  const float sample = here + (next - here) * fractional;
  const float left = before + (here - before) * fractional;
  const float right = next + (after - next) * fractional;
  *derivative = right - left;
  const float gradient = *derivative * 2.25f;
  const float scanned = sample + (gradient - sample) * timbre;
  // The offset keeps the normalized waveform inside Sine's positive range.
  const float folded = Sine(
      16.0f + scanned * (0.25f + 1.25f * fold_amount));
  return scanned + (folded - scanned) * fold_amount;
}

void Scanned::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  const float structure = parameters.timbre * parameters.timbre;
  // HARMONICS sets excitation width independently of TIMBRE's mass profile.
  const float strike_width = 1.25f + 5.0f * (1.0f - parameters.harmonics);
  if (reset_pending_ || (parameters.trigger & TRIGGER_RISING_EDGE)) {
    Excite(parameters.timbre, strike_width, 0.3f + 0.5f * parameters.accent);
    reset_pending_ = false;
  }

  const float base_frequency = min(0.24f, NoteToFrequency(parameters.note));
  const float physics_rate = 20.0f * SemitonesToRatio(0.5f * 72.0f) /
      kCorrectedSampleRate;
  const float damping = 0.99985f - 0.065f * parameters.morph *
      parameters.morph;
  const float nonlinearity = parameters.morph * parameters.morph;
  const bool driven = parameters.trigger & TRIGGER_UNPATCHED;
  int drive_index = static_cast<int>(
      parameters.timbre * static_cast<float>(kScannedMasses));
  if (drive_index >= kScannedMasses) {
    drive_index = kScannedMasses - 1;
  }

  for (size_t i = 0; i < size; ++i) {
    physics_phase_ += physics_rate;
    while (physics_phase_ >= 1.0f) {
      physics_phase_ -= 1.0f;
      Step(
          parameters.harmonics,
          structure,
          damping,
          nonlinearity,
          driven,
          drive_index);
    }

    float frequency = base_frequency;
    scan_phase_ += frequency;
    scan_phase_ -= static_cast<int>(scan_phase_);
    // Distort the scan path continuously between excitations.
    float warped_phase = scan_phase_ + 0.22f * structure * Sine(scan_phase_);
    if (warped_phase < 0.0f) {
      warped_phase += 1.0f;
    } else if (warped_phase >= 1.0f) {
      warped_phase -= 1.0f;
    }
    // Square MORPH to preserve damping control before nonlinear wavefolding.
    const float fold_amount = parameters.morph * parameters.morph;
    float derivative;
    const float shaped = ReadPickup(
        warped_phase, parameters.timbre, fold_amount, &derivative);
    out[i] = shaped * 0.8f;
    aux[i] = derivative * 2.0f;

  }
}

}  // namespace plaits
