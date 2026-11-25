// Copyright 2015 Emilie Gillet.
// Copyright 2026 Insolent Construct.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// Rings' modal resonator, rendered by Plaits' batched SVFs.

#include "beads/dsp/modal_resonator.h"

#include <algorithm>

#include "stmlib/dsp/cosine_oscillator.h"
#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/filter.h"
#include "stmlib/dsp/units.h"

#include "rings/resources.h"

namespace beads {

using namespace std;
using namespace stmlib;

// Measured or textbook modes, matched to String's level on grain-like noise.
// Tuned bar: 1:4:10 (Rossing), with short upper modes.
const ResonatorModes kResonatorMarimba = {
  {
    1.0f, 1.0f, 3.99f, 3.99f, 9.95f, 9.95f,
    17.8f, 17.8f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
  },
  {
    1.0f, 1.0f, 0.3f, 0.3f, 0.12f, 0.12f,
    0.06f, 0.06f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
  },
  {
    7.4f, 7.4f, 3.7f, 3.7f, 1.85f, 1.85f,
    0.74f, 0.74f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
  },
  0.0f, 1.0f
};

// Ideal membrane, Bessel zeros over j01; m > 0 pairs split 0.3%.
const ResonatorModes kResonatorDrum = {
  {
    1.0f, 1.0f, 1.58856f, 1.59812f, 2.12914f, 2.14196f,
    2.29542f, 2.29542f, 2.64511f, 2.66103f, 2.90854f, 2.92605f,
    3.146f, 3.16493f, 3.48965f, 3.51065f, 3.59848f, 3.59848f,
    3.63651f, 3.65839f, 4.04676f, 4.07111f, 4.11934f, 4.14413f
  },
  {
    1.0f, 1.0f, 0.848895f, 0.848895f, 0.745899f, 0.745899f,
    0.720137f, 0.720137f, 0.668485f, 0.668485f, 0.634845f, 0.634845f,
    0.607297f, 0.607297f, 0.571414f, 0.571414f, 0.561941f, 0.561941f,
    0.55734f, 0.55734f, 0.521464f, 0.521464f, 0.515591f, 0.515591f
  },
  {
    2.08f, 2.08f, 2.05977f, 2.05977f, 1.77917f, 1.77917f,
    1.37288f, 1.37288f, 1.59624f, 1.59624f, 1.52224f, 1.52224f,
    1.46366f, 1.46366f, 1.38973f, 1.38973f, 1.09649f, 1.09649f,
    1.36138f, 1.36138f, 1.29053f, 1.29053f, 1.27911f, 1.27911f
  },
  0.1f, 8.0f
};

// Bowl, n(n^2-1)/sqrt(n^2+1) from n = 2; pairs split 0.4%.
const ResonatorModes kResonatorBowl = {
  {
    0.996f, 1.004f, 2.81711f, 2.83974f, 5.40157f, 5.44495f,
    8.7355f, 8.80566f, 12.8148f, 12.9177f, 17.6379f, 17.7796f,
    23.2042f, 23.3906f, 29.5134f, 29.7504f, 36.5652f, 36.8589f,
    44.3595f, 44.7158f, 52.8964f, 53.3213f, 62.1758f, 62.6752f
  },
  {
    1.0f, 1.0f, 0.820068f, 0.820068f, 0.653257f, 0.653257f,
    0.517473f, 0.517473f, 0.41255f, 0.41255f, 0.332773f, 0.332773f,
    0.272058f, 0.272058f, 0.225437f, 0.225437f, 0.189199f, 0.189199f,
    0.160655f, 0.160655f, 0.137873f, 0.137873f, 0.119459f, 0.119459f
  },
  {
    2.4f, 2.4f, 1.28613f, 1.28613f, 0.870272f, 0.870272f,
    0.65222f, 0.65222f, 0.518251f, 0.518251f, 0.427857f, 0.427857f,
    0.362933f, 0.362933f, 0.314163f, 0.314163f, 0.276265f, 0.276265f,
    0.246022f, 0.246022f, 0.221366f, 0.221366f, 0.200907f, 0.200907f
  },
  0.0f, 1.0f
};

// Free circular plate (Rossing); m > 0 pairs split 0.35%.
const ResonatorModes kResonatorGong = {
  {
    0.9965f, 1.0035f, 1.73f, 1.73f, 2.31985f, 2.33615f,
    3.89632f, 3.92369f, 4.09562f, 4.12439f, 6.27795f, 6.32205f,
    6.68651f, 6.73348f, 7.34f, 7.34f, 8.71938f, 8.78063f,
    9.7956f, 9.86441f, 11.3601f, 11.4399f, 11.6591f, 11.7409f
  },
  {
    1.0f, 1.0f, 0.971628f, 0.971628f, 0.949559f, 0.949559f,
    0.895736f, 0.895736f, 0.889363f, 0.889363f, 0.825083f, 0.825083f,
    0.814067f, 0.814067f, 0.797703f, 0.797703f, 0.763359f, 0.763359f,
    0.738989f, 0.738989f, 0.706215f, 0.706215f, 0.70028f, 0.70028f
  },
  {
    1.75f, 1.75f, 1.3305f, 1.3305f, 1.14696f, 1.14696f,
    0.885013f, 0.885013f, 0.863211f, 0.863211f, 0.697217f, 0.697217f,
    0.67558f, 0.67558f, 0.645937f, 0.645937f, 0.591608f, 0.591608f,
    0.558163f, 0.558163f, 0.518305f, 0.518305f, 0.511617f, 0.511617f
  },
  -0.06f, 3.0f
};

/* extern */
// Chord gains: a chord about as loud as After the grains on test clouds.
const ResonatorMaterial kResonatorMaterials[kNumResonatorMaterials] = {
  { 0.5f, 0.3f, NULL, 1.03f },  // Wood: darker, mildly stretched.
  { kResonatorStructure, kResonatorBrightness, NULL, 1.72f },  // String.
  { 0.4f, 0.5f, NULL, 1.62f },  // Glass: stretched, brighter.
  { 0.8f, 0.15f, NULL, 3.22f },  // Bell: strongly stretched, dark.
  // Brightness shapes only their strike, kept under the limiter's ceiling.
  { kResonatorStructure, 0.7f, &kResonatorMarimba, 6.44f },
  { kResonatorStructure, 0.85f, &kResonatorDrum, 15.5f },  // Pitch falls.
  { kResonatorStructure, 0.5f, &kResonatorBowl, 2.61f },  // Pairs beat.
  { kResonatorStructure, 0.75f, &kResonatorGong, 8.65f },  // Pitch rises.
};

// Each step rings four times longer; 2.3 s at Medium, 48 kHz.
/* extern */
const float kResonatorRingTimes[kNumResonatorRingTimes] = {
  0.35f, kResonatorDamping, 0.65f, 0.8f
};

void ModalResonator::Init(float sample_rate) {
  for (int i = 0; i < kNumResonatorModes / 8; ++i) {
    odd_[i].Init();
    even_[i].Init();
  }
  set_frequency(220.0f / sample_rate);
  set_structure(kResonatorStructure);
  set_brightness(kResonatorBrightness);
  set_damping(kResonatorDamping);
  set_position(kResonatorPosition);
  set_amount(1.0f);
  modes_ = NULL;
  glide_ = 0.0f;
  sample_period_ = 1.0f / sample_rate;
  for (int v = 0; v < kNumChordVoices; ++v) {
    chord_ratio_[v] = 0.0f;
    chord_level_[v] = 0.0f;
    chord_peak_[v] = 0.0f;
    chord_glide_[v] = 0.0f;
  }
  chord_modes_ = NULL;
  limiter_.Init(
      1.0f / (sample_rate * 0.5f), kResonatorPeak * kResonatorHeadroom);
  // Init leaves its peak 24 dB over the threshold.
  limiter_.Rest();
  peak_gain_ = 0.0f;
}

void ModalResonator::Render(const float* in, FloatFrame* frames, size_t size) {
  float l[size];
  float r[size];
  Process(in, l, r, size);
  FloatFrame resonance[size];
  for (size_t i = 0; i < size; ++i) {
    resonance[i].l = l[i] * kResonatorHeadroom;
    resonance[i].r = r[i] * kResonatorHeadroom;
  }
  limiter_.Process(resonance, size);
  const float gain = amount_ * (1.0f / kResonatorHeadroom);
  for (size_t i = 0; i < size; ++i) {
    frames[i].l += resonance[i].l * gain;
    frames[i].r += resonance[i].r * gain;
  }
}

// A mode's decay at f, Q = 1 + f k: |z|^2 = 1 - 2y; it rises once, then falls.
inline float ModeDecay(float f, float k) {
  f = min(f, 0.49f);
  const float g = OnePole::tan<FREQUENCY_FAST>(f);
  const float r = 1.0f / (1.0f + f * k);
  return g * r / (1.0f + g * r + g * g);
}

float ModalResonator::LoopGain(const ResonatorBound& bound, size_t size) {
  // A mode's Q is 1 + f r q d; a strike's glide falls to 0 from either side.
  const float glide = modes_ ? modes_->glide : 0.0f;
  const float f = frequency_ * (1.0f + max(glide, 0.0f));
  const float q = 500.0f * Interpolate(rings::lut_4_decades, damping_, 256.0f);
  const float gain = min(
      bound.peak + f * q * bound.peak_slope + bound.spill,
      bound.sum + f * q * bound.sum_slope) * kResonatorBoundMargin;
  // What a change of tuning leaves ringing falls no faster than the slowest
  // mode, at the lowest or the highest mode's frequency.
  const float k = q * bound.largest_decay;
  const float y = min(
      ModeDecay(frequency_ * (1.0f + min(glide, 0.0f)) * bound.lowest_ratio, k),
      ModeDecay(f * bound.highest_ratio, k));
  peak_gain_ = max(gain, peak_gain_ / (1.0f + y * float(size)));
  // Only for the object it was worked out for; Amount scales the mix only.
  const bool known = bound.structure == structure_ &&
      bound.brightness == brightness_ && bound.position == position_ &&
      bound.modes == modes_ && amount_ > 0.0f && peak_gain_ > 0.0f;
  return known ? 1.0f / (amount_ * peak_gain_) : 0.0f;
}

// Gain of the strum's two-pole low-pass at (f / cutoff)^2 = x2.
static inline float StrikeLowPass(float x2, float damping) {
  const float d = 1.0f - x2;
  return 1.0f / Sqrt(d * d + x2 * damping);
}

void ModalResonator::Strike(
    int voice,
    size_t offset,
    float gain,
    float* strikes,
    size_t size) {
  UpdateChordModes();
  // Rings' strum level; its low-pass is folded into the mode amplitudes.
  const float brightness = brightness_ * (2.0f - brightness_);
  const float cutoff = min(frequency_ * chord_ratio_[voice] *
      SemitonesToRatio((brightness - 0.5f) * 96.0f), 0.499f);
  strikes[voice * size + offset] +=
      gain * 0.25f * SemitonesToRatio(cutoff * cutoff * 24.0f) / cutoff;
  chord_level_[voice] += gain;
  chord_peak_[voice] = max(chord_peak_[voice], chord_level_[voice]);
  chord_glide_[voice] = modes_ ? modes_->glide : 0.0f;
}

void ModalResonator::StrikeVoice(
    float ratio,
    size_t offset,
    float gain,
    float* strikes,
    size_t size) {
  CONSTRAIN(ratio, 1.0f / 16.0f, 16.0f);
  int nearest = -1;
  int quietest = 0;
  float closest = 0.0f;
  float quiet = 2.0f;
  for (int v = 0; v < kNumChordVoices; ++v) {
    const float r = chord_ratio_[v];
    if (r != 0.0f) {
      const float closeness = r > ratio ? ratio / r : r / ratio;
      if (closeness > closest) {
        closest = closeness;
        nearest = v;
      }
    }
    // Under its own loudest; a voice never struck audibly is silent.
    const float relative = chord_peak_[v] > 0.0f
        ? chord_level_[v] / chord_peak_[v]
        : 0.0f;
    if (relative < quiet) {
      quiet = relative;
      quietest = v;
    }
  }
  int voice = nearest;
  if (closest < kChordNoteTolerance && quiet < kChordRetuneLevel) {
    // Steal the quietest; a ringing voice keeps its tuning. Its residue,
    // 41 dB under its loudest, is not counted against the new note.
    chord_ratio_[quietest] = ratio;
    chord_level_[quietest] = 0.0f;
    chord_peak_[quietest] = 0.0f;
    voice = quietest;
  }
  Strike(voice, offset, gain, strikes, size);
}

void ModalResonator::StrikeChord(float* strikes, size_t size) {
  for (int v = 0; v < kNumChordVoices; ++v) {
    if (chord_ratio_[v] != 0.0f) {
      Strike(v, 0, 1.0f, strikes, size);
    }
  }
}

void ModalResonator::RenderChord(
    const float* strikes,
    FloatFrame* frames,
    size_t size) {
  UpdateChordModes();
  float l[size];
  float r[size];
  fill(&l[0], &l[size], 0.0f);
  fill(&r[0], &r[size], 0.0f);

  // One voice's modes over its frequency, with Rings' strum low-pass.
  const float q = 500.0f * Interpolate(rings::lut_4_decades, damping_, 256.0f);
  const float brightness = brightness_ * (2.0f - brightness_);
  const float cutoff = SemitonesToRatio((brightness - 0.5f) * 96.0f);
  const float rolloff = 1.0f / (cutoff * cutoff);
  const float strum_damping = 1.0f / (1.5f * 1.5f);
  float ratio[2][plaits::kModeBatchSize];
  float quality[2][plaits::kModeBatchSize];
  float amplitude[2][plaits::kModeBatchSize];
  CosineOscillator amplitudes;
  amplitudes.Init<COSINE_OSCILLATOR_APPROXIMATE>(position_);
  if (modes_) {
    const ResonatorModes& modes = *modes_;
    for (int i = 0; i < plaits::kModeBatchSize; ++i) {
      const float pickup = amplitudes.Next() * 0.125f;
      for (int side = 0; side < 2; ++side) {
        const int mode = 2 * i + side;
        const float n = modes.ratio[mode];
        ratio[side][i] = n;
        quality[side][i] = q * modes.decay[mode];
        amplitude[side][i] = pickup * modes.gain[mode] *
            StrikeLowPass(n * n * rolloff, strum_damping);
      }
    }
  } else {
    // Rings' Resonator::ComputeFilters, its first batch over the frequency.
    float stiffness = Interpolate(rings::lut_stiffness, structure_, 256.0f);
    float harmonic = 1.0f;
    float stretch_factor = 1.0f;
    float mode_quality = q;
    float brightness_attenuation = 1.0f - structure_;
    brightness_attenuation *= brightness_attenuation;
    brightness_attenuation *= brightness_attenuation;
    brightness_attenuation *= brightness_attenuation;
    const float b = brightness_ * (1.0f - 0.2f * brightness_attenuation);
    float q_loss = b * (2.0f - b) * 0.85f + 0.15f;
    const float q_loss_damping_rate = structure_ * (2.0f - structure_) * 0.1f;
    for (int i = 0; i < 8; ++i) {
      const float n = harmonic * stretch_factor;
      ratio[i & 1][i >> 1] = n;
      quality[i & 1][i >> 1] = mode_quality;
      amplitude[i & 1][i >> 1] = amplitudes.Next() * 0.125f *
          StrikeLowPass(n * n * rolloff, strum_damping);
      stretch_factor += stiffness;
      if (stiffness < 0.0f) {
        stiffness *= 0.93f;
      } else {
        stiffness *= 0.98f;
      }
      q_loss += q_loss_damping_rate * (1.0f - q_loss);
      harmonic += 1.0f;
      mode_quality *= q_loss;
    }
  }

  const float glide_decay = modes_
      ? min(modes_->glide_rate * sample_period_ * float(size), 1.0f)
      : 0.0f;
  for (int v = 0; v < kNumChordVoices; ++v) {
    if (chord_level_[v] <= kChordSilence * chord_peak_[v]) {
      continue;
    }
    const float frequency =
        frequency_ * chord_ratio_[v] * (1.0f + chord_glide_[v]);
    chord_glide_[v] -= chord_glide_[v] * glide_decay;
    float mode_f[2][plaits::kModeBatchSize];
    float mode_q[2][plaits::kModeBatchSize];
    float mode_a[2][plaits::kModeBatchSize];
    for (int side = 0; side < 2; ++side) {
      for (int i = 0; i < plaits::kModeBatchSize; ++i) {
        float partial_frequency = frequency * ratio[side][i];
        float a = amplitude[side][i];
        if (partial_frequency >= 0.49f) {
          partial_frequency = 0.49f;
          a = 0.0f;
        }
        mode_f[side][i] = partial_frequency;
        mode_q[side][i] = 1.0f + partial_frequency * quality[side][i];
        mode_a[side][i] = a;
      }
    }
    const float* in = &strikes[v * size];
    odd_[v].Process<FILTER_MODE_BAND_PASS, true>(
        mode_f[0], mode_q[0], mode_a[0], in, l, size);
    even_[v].Process<FILTER_MODE_BAND_PASS, true>(
        mode_f[1], mode_q[1], mode_a[1], in, r, size);
    // The fundamental's pole radius, squared, from the SVF's coefficients;
    // 2% slower, so that float rounding never makes it fall too fast.
    const float g = OnePole::tan<FREQUENCY_FAST>(min(frequency, 0.49f));
    const float r_over_q = g / (1.0f + frequency * q);
    const float radius = 1.0f - 0.98f * 2.0f * r_over_q /
        (1.0f + r_over_q + g * g);
    for (size_t i = 0; i < size; i += 2) {
      chord_level_[v] *= radius;
    }
  }
  // Limited at the headroom as Render does, then back to unity.
  FloatFrame resonance[size];
  for (size_t i = 0; i < size; ++i) {
    resonance[i].l = kResonatorHeadroom * l[i];
    resonance[i].r = kResonatorHeadroom * r[i];
  }
  limiter_.Process(resonance, size);
  const float gain = amount_ / kResonatorHeadroom;
  for (size_t i = 0; i < size; ++i) {
    frames[i].l += gain * resonance[i].l;
    frames[i].r += gain * resonance[i].r;
  }
}

void ModalResonator::Process(
    const float* in,
    float* out,
    float* aux,
    size_t size) {
  if (modes_) {
    ProcessModes(in, out, aux, size);
    return;
  }
  fill(&out[0], &out[size], 0.0f);
  fill(&aux[0], &aux[size], 0.0f);

  // Rings' Resonator::ComputeFilters.
  float stiffness = Interpolate(rings::lut_stiffness, structure_, 256.0f);
  float harmonic = frequency_;
  float stretch_factor = 1.0f;
  float q = 500.0f * Interpolate(rings::lut_4_decades, damping_, 256.0f);
  float brightness_attenuation = 1.0f - structure_;
  brightness_attenuation *= brightness_attenuation;
  brightness_attenuation *= brightness_attenuation;
  brightness_attenuation *= brightness_attenuation;
  float brightness = brightness_ * (1.0f - 0.2f * brightness_attenuation);
  float q_loss = brightness * (2.0f - brightness) * 0.85f + 0.15f;
  float q_loss_damping_rate = structure_ * (2.0f - structure_) * 0.1f;

  CosineOscillator amplitudes;
  amplitudes.Init<COSINE_OSCILLATOR_APPROXIMATE>(position_);

  float mode_f[2][plaits::kModeBatchSize];
  float mode_q[2][plaits::kModeBatchSize];
  float mode_a[2][plaits::kModeBatchSize];
  plaits::ResonatorSvf<plaits::kModeBatchSize>* odd = &odd_[0];
  plaits::ResonatorSvf<plaits::kModeBatchSize>* even = &even_[0];
  for (int batch = 0; batch < kNumResonatorModes / 8; ++batch) {
    bool audible = false;
    for (int i = 0; i < 8; ++i) {
      float partial_frequency = harmonic * stretch_factor;
      // Rings' input gain of 0.125 is folded into the mode amplitude.
      float amplitude = amplitudes.Next() * 0.125f;
      if (partial_frequency >= 0.49f) {
        partial_frequency = 0.49f;
        amplitude = 0.0f;
      } else {
        audible = true;
      }
      mode_f[i & 1][i >> 1] = partial_frequency;
      mode_q[i & 1][i >> 1] = 1.0f + partial_frequency * q;
      mode_a[i & 1][i >> 1] = amplitude;
      stretch_factor += stiffness;
      if (stiffness < 0.0f) {
        stiffness *= 0.93f;
      } else {
        stiffness *= 0.98f;
      }
      q_loss += q_loss_damping_rate * (1.0f - q_loss);
      harmonic += frequency_;
      q *= q_loss;
    }
    if (audible) {
      odd->Process<FILTER_MODE_BAND_PASS, true>(
          mode_f[0], mode_q[0], mode_a[0], in, out, size);
      even->Process<FILTER_MODE_BAND_PASS, true>(
          mode_f[1], mode_q[1], mode_a[1], in, aux, size);
    }
    ++odd;
    ++even;
  }
}

void ModalResonator::ProcessModes(
    const float* in,
    float* out,
    float* aux,
    size_t size) {
  fill(&out[0], &out[size], 0.0f);
  fill(&aux[0], &aux[size], 0.0f);

  const ResonatorModes& modes = *modes_;
  const float frequency = frequency_ * (1.0f + glide_);
  glide_ -= glide_ * min(modes.glide_rate * sample_period_ * float(size), 1.0f);
  const float q = 500.0f * Interpolate(rings::lut_4_decades, damping_, 256.0f);

  CosineOscillator amplitudes;
  amplitudes.Init<COSINE_OSCILLATOR_APPROXIMATE>(position_);

  float mode_f[2][plaits::kModeBatchSize];
  float mode_q[2][plaits::kModeBatchSize];
  float mode_a[2][plaits::kModeBatchSize];
  const float* ratio = modes.ratio;
  const float* decay = modes.decay;
  const float* gain = modes.gain;
  plaits::ResonatorSvf<plaits::kModeBatchSize>* odd = &odd_[0];
  plaits::ResonatorSvf<plaits::kModeBatchSize>* even = &even_[0];
  for (int batch = 0; batch < kNumResonatorModes / 8; ++batch) {
    bool audible = false;
    for (int i = 0; i < plaits::kModeBatchSize; ++i) {
      // A pair shares its pickup; its left and right modes may differ.
      const float pickup = amplitudes.Next() * 0.125f;
      for (int side = 0; side < 2; ++side) {
        float partial_frequency = frequency * *ratio++;
        float amplitude = pickup * *gain;
        if (partial_frequency >= 0.49f) {
          partial_frequency = 0.49f;
          amplitude = 0.0f;
        } else if (*gain != 0.0f) {
          audible = true;
        }
        mode_f[side][i] = partial_frequency;
        mode_q[side][i] = 1.0f + partial_frequency * q * *decay++;
        mode_a[side][i] = amplitude;
        ++gain;
      }
    }
    if (audible) {
      odd->Process<FILTER_MODE_BAND_PASS, true>(
          mode_f[0], mode_q[0], mode_a[0], in, out, size);
      even->Process<FILTER_MODE_BAND_PASS, true>(
          mode_f[1], mode_q[1], mode_a[1], in, aux, size);
    } else {
      // A silent batch keeps no ring for a later object.
      odd->Init();
      even->Init();
    }
    ++odd;
    ++even;
  }
}

/* static */
void ModalResonator::Bound(
    float structure,
    float brightness,
    float position,
    const ResonatorModes* modes,
    ResonatorBound* bound) {
  // Each mode's ratio, |level| and Q factor as Process sets them, before any
  // is cut at Nyquist; one list per side.
  float ratio[2][kNumResonatorModes / 2];
  float level[2][kNumResonatorModes / 2];
  float decay[2][kNumResonatorModes / 2];
  CosineOscillator amplitudes;
  amplitudes.Init<COSINE_OSCILLATOR_APPROXIMATE>(position);
  if (modes) {
    for (int i = 0; i < kNumResonatorModes / 2; ++i) {
      const float pickup = amplitudes.Next() * 0.125f;
      for (int side = 0; side < 2; ++side) {
        ratio[side][i] = modes->ratio[2 * i + side];
        level[side][i] = fabsf(pickup * modes->gain[2 * i + side]);
        decay[side][i] = modes->decay[2 * i + side];
      }
    }
  } else {
    // Rings' Resonator::ComputeFilters, with q = 1.
    float stiffness = Interpolate(rings::lut_stiffness, structure, 256.0f);
    float stretch_factor = 1.0f;
    float q = 1.0f;
    float brightness_attenuation = 1.0f - structure;
    brightness_attenuation *= brightness_attenuation;
    brightness_attenuation *= brightness_attenuation;
    brightness_attenuation *= brightness_attenuation;
    const float b = brightness * (1.0f - 0.2f * brightness_attenuation);
    float q_loss = b * (2.0f - b) * 0.85f + 0.15f;
    const float q_loss_damping_rate = structure * (2.0f - structure) * 0.1f;
    for (int i = 0; i < kNumResonatorModes; ++i) {
      ratio[i & 1][i >> 1] = float(i + 1) * stretch_factor;
      level[i & 1][i >> 1] = fabsf(amplitudes.Next() * 0.125f);
      decay[i & 1][i >> 1] = q;
      stretch_factor += stiffness;
      if (stiffness < 0.0f) {
        stiffness *= 0.93f;
      } else {
        stiffness *= 0.98f;
      }
      q_loss += q_loss_damping_rate * (1.0f - q_loss);
      q *= q_loss;
    }
  }
  bound->peak = bound->peak_slope = bound->sum = bound->sum_slope = 0.0f;
  bound->spill = 0.0f;
  bound->lowest_ratio = bound->highest_ratio = ratio[0][0];
  bound->largest_decay = 0.0f;
  for (int side = 0; side < 2; ++side) {
    float sum = 0.0f;
    float sum_slope = 0.0f;
    for (int k = 0; k < kNumResonatorModes / 2; ++k) {
      if (level[side][k] == 0.0f) {
        continue;
      }
      const float slope = level[side][k] * ratio[side][k] * decay[side][k];
      bound->peak = max(bound->peak, level[side][k]);
      bound->peak_slope = max(bound->peak_slope, slope);
      sum += level[side][k];
      sum_slope += slope;
      bound->lowest_ratio = min(bound->lowest_ratio, ratio[side][k]);
      bound->highest_ratio = max(bound->highest_ratio, ratio[side][k]);
      bound->largest_decay = max(bound->largest_decay, decay[side][k]);
      // Off its peak mode j adds at most a / |x - 1 / x|, x its distance;
      // nearer mode k than j, that is sqrt(r) / |r - 1|, r = f_k / f_j.
      float spill = 0.0f;
      for (int j = 0; j < kNumResonatorModes / 2; ++j) {
        if (j == k || level[side][j] == 0.0f) {
          continue;
        }
        const float r = ratio[side][k] / ratio[side][j];
        const float distance = fabsf(r - 1.0f);
        // Two modes on one pitch: the sum of all peaks bounds it instead.
        spill += distance < 1.0e-4f
            ? kResonatorNoSpill
            : level[side][j] * Sqrt(r) / distance;
      }
      bound->spill = max(bound->spill, spill);
    }
    bound->sum = max(bound->sum, sum);
    bound->sum_slope = max(bound->sum_slope, sum_slope);
  }
  bound->structure = structure;
  bound->brightness = brightness;
  bound->position = position;
  bound->modes = modes;
}

}  // namespace beads
