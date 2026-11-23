// Copyright 1995-2023 Perry R. Cook and Gary P. Scavone.
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
// Shakers: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/shakers_engine.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"
#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

const float kShakerData[] = {
  3200.0f, 0.96f, 1.0f, 3000.0f, 0.7f, 1.0f,
  5500.0f, 0.6f, 1.0f, 2300.0f, 5600.0f, 8100.0f,
  0.96f, 0.99f, 0.99f, 0.1f, 0.8f, 1.0f,
  2500.0f, 5300.0f, 6500.0f, 8300.0f, 9800.0f, 0.99f,
  0.99f, 0.99f, 0.99f, 0.99f, 1.0f, 1.0f,
  1.0f, 0.5f, 0.3f, 2800.0f, 2240.0f, 3360.0f,
  0.995f, 0.995f, 0.995f, 1.0f, 1.0f, 1.0f,
  1046.6f, 1174.8f, 1397.0f, 1568.0f, 1760.0f, 2093.3f,
  2350.0f, 0.996f, 0.996f, 0.996f, 0.996f, 0.996f,
  0.996f, 0.996f, 1.0f, 1.0f, 1.0f, 1.0f,
  1.0f, 1.0f, 1.0f, 370.0f, 1025.0f, 1424.0f,
  2149.0f, 3596.0f, 0.99f, 0.992f, 0.992f, 0.992f,
  0.992f, 1.0f, 1.8f, 1.8f, 1.8f, 1.8f,
  5500.0f, 0.6f, 1.0f, 800.0f, 0.95f, 1.0f,
  6460.0f, 0.932f, 1.0f, 9000.0f, 0.843f, 1.0f,
  2123.0f, 4518.0f, 8856.0f, 10753.0f, 1708.0f, 8863.0f,
  9045.0f, 0.997f, 0.997f, 0.997f, 0.997f, 0.9995f,
  0.9995f, 0.9995f, 1.0f, 0.8f, 0.6f, 0.4f,
  1.0f, 0.8f, 0.5f, 450.0f, 600.0f, 750.0f,
  0.9985f, 0.9985f, 0.9985f, 1.0f, 1.0f, 1.0f,
  2500.0f, 4000.0f, 0.97f, 0.97f, 1.0f, 1.0f,
  3200.0f, 8000.0f, 0.99f, 0.992f, 1.0f, 1.0f,
};

// Measured resonances, transcribed from STK's Shakers.cpp. Frequencies in Hz,
















































// Mug plus a quarter: four mug modes then three coin modes, which is how
















enum ShakerMechanism {
  SHAKER_MECHANISM_SHAKE = 0,
  SHAKER_MECHANISM_RATCHET = 1,
  SHAKER_MECHANISM_WATER = 2
};

// The firmware is bare-metal, so libm's powf/logf/cosf cannot link -- they pull
inline float FastLog2(float x) {
  union { float f; uint32_t i; } u = { x };
  const float e = static_cast<float>((u.i >> 23) & 0xFFu) - 127.0f;
  u.i = (u.i & 0x007FFFFFu) | 0x3F800000u;   // mantissa -> [1, 2)
  const float m = u.f;
  return e + (-1.7417939f + (2.8212026f + (-1.4699568f
             + (0.44717955f - 0.056570851f * m) * m) * m) * m);
}

// cos(2*pi*x) from the shared sine LUT. Sine(t) is sin(2*pi*t), so this is
inline float CosTwoPi(float x) {
  return Sine(x + 0.25f);
}

// Rate-corrects a per-sample decay or filter radius from 44.1 kHz to 48 kHz so
inline float RateCorrect(float coefficient) {
  if (coefficient <= 0.0f) {
    return 0.0f;
  }
  if (coefficient >= 1.0f) {
    return 1.0f;
  }
  const float z = (coefficient - 1.0f) / (coefficient + 1.0f);
  const float z2 = z * z;
  const float ln_c = 2.0f * z *
      (1.0f + z2 * (1.0f / 3.0f + z2 * (0.2f + z2 * (1.0f / 7.0f))));
  const float y = kShakersReferenceRate / kSampleRate * ln_c;
  // exp(y) for y in about [-0.5, 0]; terms to y^6 hold ~1e-6 over that range.
  return 1.0f + y * (1.0f + y * (0.5f + y * (1.0f / 6.0f + y *
      (1.0f / 24.0f + y * (1.0f / 120.0f + y * (1.0f / 720.0f))))));
}

inline float Noise() {
  return 2.0f * Random::GetFloat() - 1.0f;
}

}  // namespace

const ShakerPreset kShakerPresets[kShakersNumInstruments] = {
  //                        n  freq          radii         gains       objects  sysdec  snddec  gain  dscale vary  mask  b0     b1     b2    mech                       ratchet  makeup
  { 1, 0,      1,      2,      25.0f, 0.999f,  0.95f,  4.0f, 0.97f, 0.0f,  0x00, 1.0f, -1.0f,  0.0f, SHAKER_MECHANISM_SHAKE,   0.0f, 1.788f },
  { 1, 3,      4,      5,     512.0f, 0.997f,  0.96f,  8.0f, 0.97f, 0.0f,  0x00, 1.0f, -1.0f,  0.0f, SHAKER_MECHANISM_SHAKE,   0.0f, 0.248f },
  { 1, 6,      7,      8,      64.0f, 0.999f,  0.96f,  4.0f, 0.94f, 0.0f,  0x00, 1.0f,  0.0f, -1.0f, SHAKER_MECHANISM_SHAKE,   0.0f, 0.237f },
  { 3, 9,  12,  15,  32.0f, 0.9985f, 0.95f,  1.0f, 0.95f, 0.05f, 0x06, 1.0f,  0.0f, -1.0f, SHAKER_MECHANISM_SHAKE,   0.0f, 6.705f },
  { 5, 18,      23,      28,      32.0f, 0.9994f, 0.97f,  1.0f, 0.9f,  0.03f, 0x1f, 1.0f,  0.0f, -1.0f, SHAKER_MECHANISM_SHAKE,   0.0f, 2.564f },
  { 3, 33,      36,      39,       1.2f, 0.9999f, 0.9f,   0.4f, 0.7f,  0.2f,  0x07, 1.0f,  0.0f,  0.0f, SHAKER_MECHANISM_SHAKE,   0.0f, 8.183f },
  { 7, 42,    49,    56,     1.2f, 0.9999f, 0.95f,  0.5f, 0.7f,  0.0f,  0x00, 1.0f,  0.0f, -1.0f, SHAKER_MECHANISM_SHAKE,   0.0f, 9.049f },
  { 5, 63,     68,     73,     48.0f, 0.999f,  0.97f,  0.5f, 0.95f, 0.0f,  0x00, 1.0f,  0.0f, -1.0f, SHAKER_MECHANISM_SHAKE,   0.0f, 2.654f },
  { 1, 78,        79,        80,         2.0f, 0.998f,  0.96f,  6.0f, 0.96f, 0.0f,  0x00, 1.0f,  0.0f, -1.0f, SHAKER_MECHANISM_SHAKE,   0.0f, 0.493f },
  { 1, 81,      82,      83,       7.0f, 0.99806f,0.95f,  4.0f, 0.96f, 0.0f,  0x00, 1.0f, -1.0f,  0.0f, SHAKER_MECHANISM_SHAKE,   0.0f, 2.33f },
  { 1, 84,    85,    86,    23.0f, 0.9965f, 0.98f,  4.0f, 0.95f, 0.11f, 0x01, 1.0f,  0.0f, -1.0f, SHAKER_MECHANISM_SHAKE,   0.0f, 0.991f },
  { 1, 87, 88, 89,1600.0f,0.99586f,0.98f,  4.0f, 0.95f, 0.18f, 0x01, 1.0f,  0.0f, -1.0f, SHAKER_MECHANISM_SHAKE,   0.0f, 0.843f },
  { 7, 90,         97,         104,          3.0f, 0.9995f, 0.97f,  0.8f, 0.95f, 0.0f,  0x00, 1.0f,  0.0f, -1.0f, SHAKER_MECHANISM_SHAKE,   0.0f, 10.062f },
  { 3, 111,       114,       117,       10.0f, 0.996f,  0.95f,  1.0f, 0.8f,  0.0f,  0x00,-1.0f,  0.0f,  1.0f, SHAKER_MECHANISM_WATER,   0.0f, 50.0f },
  { 2, 120,       122,       124,      128.0f, 0.999f,  0.95f,  0.4f, 0.95f, 0.0f,  0x00, 1.0f,  0.0f, -1.0f, SHAKER_MECHANISM_RATCHET, kShakersGuiroRatchetDelta, 9.51f },
  { 2, 126,      128,      130,     128.0f, 0.999f,  0.95f,  0.4f, 0.95f, 0.0f,  0x00, 1.0f,  0.0f, -1.0f, SHAKER_MECHANISM_RATCHET, kShakersWrenchRatchetDelta, 17.74f },
};

void Shakers::Init(BufferAllocator* allocator) {
  (void) allocator;
  instrument_ = -1;
  note_ratio_ = 1.0f;
  Reset();
}

void Shakers::Reset() {
  shake_energy_ = 0.0f;
  sound_level_ = 0.0f;
  ratchet_count_ = 0.0f;
  ratchet_delta_ = kShakersGuiroRatchetDelta;
  eq_x1_ = 0.0f;
  eq_x2_ = 0.0f;
  for (int i = 0; i < kShakersMaxResonances; ++i) {
    y1_[i] = 0.0f;
    y2_[i] = 0.0f;
  }
  const int instrument = instrument_ < 0 ? 0 : instrument_;
  instrument_ = -1;
  SetInstrument(instrument);
}

void Shakers::SetInstrument(int instrument) {
  if (instrument == instrument_) {
    return;
  }
  instrument_ = instrument;
  const ShakerPreset& p = kShakerPresets[instrument];

  for (int i = 0; i < p.num_resonances; ++i) {
    frequency_[i] = kShakerData[p.frequencies_offset + i];
    radius_[i] = RateCorrect(kShakerData[p.radii_offset + i]);
    // Upstream's resonators are all-pole with a numerator of 1, so their peak
    normalization_[i] = 1.0f - radius_[i] * radius_[i];
    gain_[i] = kShakerData[p.gains_offset + i];
    y1_[i] = 0.0f;
    y2_[i] = 0.0f;
    water_frequency_[i] = kShakerData[p.frequencies_offset + i];
  }
  // A water drop starts silent and is born by the mechanism.
  if (p.mechanism == SHAKER_MECHANISM_WATER) {
    for (int i = 0; i < p.num_resonances; ++i) {
      gain_[i] = 0.0f;
    }
  }
  shake_energy_ = 0.0f;
  sound_level_ = 0.0f;
  ratchet_count_ = 0.0f;
  ratchet_delta_ = p.ratchet_delta;
  eq_x1_ = 0.0f;
  eq_x2_ = 0.0f;
}

void Shakers::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  // PhISEM carries its own energy envelope; a second one from the LPG would
  *already_enveloped = true;

  int instrument = static_cast<int>(
      parameters.harmonics * kShakersNumInstruments);
  CONSTRAIN(instrument, 0, kShakersNumInstruments - 1);
  SetInstrument(instrument);

  const ShakerPreset& p = kShakerPresets[instrument];

  // MACRO is upstream's object-count control verbatim, and its own mapping
  num_objects_ = 2.0f * 0.5f * p.num_objects + 1.1f;
  // ln(x) = log2(x) * ln(2)
  current_gain_ = FastLog2(num_objects_) * 0.69314718f * p.gain /
      num_objects_;
  // 48 kHz gives 8.8% more chances per second for a collision than the rate
  const float object_threshold = num_objects_ *
      (kShakersReferenceRate / kSampleRate);

  // MORPH is upstream's decay control, likewise centred on the measured value.
  const float base_decay = RateCorrect(p.system_decay);
  system_decay_ = base_decay + 2.0f * (parameters.morph - 0.5f) *
      p.decay_scale * (1.0f - base_decay);
  CONSTRAIN(system_decay_, 0.0f, 0.99999f);
  const float sound_decay = RateCorrect(p.sound_decay);

  // The note transposes every resonance. Retuning per block rather than per
  note_ratio_ = SemitonesToRatio(parameters.note - kShakersReferenceNote);
  for (int i = 0; i < p.num_resonances; ++i) {
    float f = frequency_[i] * note_ratio_;
    CONSTRAIN(f, 1.0f, 0.49f * kSampleRate);
    a1_[i] = -2.0f * radius_[i] * CosTwoPi(f / kSampleRate);
    a2_[i] = radius_[i] * radius_[i];
  }

  // TIMBRE is how hard it is being shaken. Upstream injects energy from a MIDI
  const bool shaking = parameters.trigger &
      (TRIGGER_HIGH | TRIGGER_UNPATCHED);
  const float shake_rate = shaking
      ? parameters.timbre * parameters.timbre * 0.02f
      : 0.0f;
  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    if (p.mechanism == SHAKER_MECHANISM_RATCHET) {
      ratchet_count_ += 8.0f;
      ratchet_delta_ = p.ratchet_delta * (1.0f + 8.0f * parameters.timbre);
    } else {
      shake_energy_ = kShakersMaxShake;
    }
  }

  for (size_t i = 0; i < size; ++i) {
    float input = 0.0f;
    float sample_note_ratio = note_ratio_;

    if (p.mechanism == SHAKER_MECHANISM_RATCHET) {
      // A ratchet ramps its energy down per tooth and resets, which is the
      if (shake_rate > 0.0f && ratchet_count_ < 1.0f) {
        ratchet_count_ = 1.0f;
        ratchet_delta_ = p.ratchet_delta * (1.0f + 8.0f * parameters.timbre);
      }
      if (ratchet_count_ > 0.0f) {
        shake_energy_ -= ratchet_delta_ + 0.002f * shake_energy_;
        if (shake_energy_ < 0.0f) {
          shake_energy_ = 1.0f;
          ratchet_count_ -= 1.0f;
        }
        if (Random::GetFloat() * 1024.0f < object_threshold) {
          sound_level_ += shake_energy_ * shake_energy_;
        }
        input = sound_level_ * Noise() * shake_energy_;
      }
    } else {
      shake_energy_ += shake_rate * kShakersMaxShake * 0.1f;
      if (shake_energy_ > kShakersMaxShake) {
        shake_energy_ = kShakersMaxShake;
      }
      if (shake_energy_ < kShakersMinEnergy) {
        shake_energy_ = 0.0f;
      }
      shake_energy_ *= system_decay_;

      if (p.mechanism == SHAKER_MECHANISM_WATER) {
        // Each drop is born into whichever resonator has fully decayed, tuned
        if (Random::GetFloat() * 32767.0f < object_threshold) {
          sound_level_ = shake_energy_;
          const int j = static_cast<int>(Random::GetFloat() * 3.0f) % 3;
          for (int k = 0; k < 3; ++k) {
            const int slot = (j + k) % 3;
            if (gain_[slot] == 0.0f) {
              const float centre = kShakerData[p.frequencies_offset + 1] *
                  (0.75f + 0.25f * float(slot) + 0.25f * Noise());
              water_frequency_[slot] = centre;
              gain_[slot] = fabsf(Noise());
              break;
            }
          }
        }
        for (int k = 0; k < p.num_resonances; ++k) {
          gain_[k] *= radius_[k];
          if (gain_[k] > 0.001f) {
            water_frequency_[k] *= kShakersWaterFreqSweep;
            float f = water_frequency_[k] * sample_note_ratio;
            CONSTRAIN(f, 1.0f, 0.49f * kSampleRate);
            a1_[k] = -2.0f * radius_[k] *
                CosTwoPi(f / kSampleRate);
          } else {
            gain_[k] = 0.0f;
          }
        }
        input = sound_level_;
      } else if (Random::GetFloat() * 1024.0f < object_threshold) {
        sound_level_ += shake_energy_;
        input = sound_level_;
        // Instruments whose colliding objects are themselves resonant --
        if (p.vary_factor > 0.0f) {
          for (int k = 0; k < p.num_resonances; ++k) {
            if (p.vary_mask & (1 << k)) {
              float f = frequency_[k] * sample_note_ratio *
                  (1.0f + p.vary_factor * Noise());
              CONSTRAIN(f, 1.0f, 0.49f * kSampleRate);
              a1_[k] = -2.0f * radius_[k] *
                  CosTwoPi(f / kSampleRate);
            }
          }
        }
      }
    }

    sound_level_ *= sound_decay;

    // AUX carries the bare collision train before any resonance -- the impact
    aux[i] = SoftClip(input * current_gain_ * p.makeup);

    float summed = 0.0f;
    for (int k = 0; k < p.num_resonances; ++k) {
      const float y = input * gain_[k] * current_gain_ * normalization_[k]
          - a1_[k] * y1_[k] - a2_[k] * y2_[k];
      y2_[k] = y1_[k];
      y1_[k] = y;
      summed += y;
    }

    // The equalizer is where the zeros live. Upstream's three numerator sets
    const float equalized = (p.eq_b0 * summed + p.eq_b1 * eq_x1_ +
        p.eq_b2 * eq_x2_) * p.makeup;
    eq_x2_ = eq_x1_;
    eq_x1_ = summed;

    // The equalizer's two-sample difference can add 6 dB on its own, and a
    out[i] = SoftClip(equalized);
  }
}

}  // namespace plaits
