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
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// StruckDrum: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/struck_drum_engine.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"
#include "stmlib/utils/random.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

inline float ApplyMacro(float stock_value, float minimum_value,
    float maximum_value, float macro) {
  const float amount = macro * 2.0f;
  return macro < 0.5f
      ? minimum_value + (stock_value - minimum_value) * amount
      : stock_value + (maximum_value - stock_value) * (amount - 1.0f);
}

}  // namespace

namespace {

// Braids' wav_sine is not a unit-amplitude, zero-mean -cos(2*pi*phase) --
const float kBraidsSinePhaseOffset = 0.75f;
const float kBraidsSineAmplitude = 32638.0f / 32768.0f;
const float kBraidsSineDcOffset = 127.0f / 32768.0f;

inline float BraidsSine(float phase) {
  phase += kBraidsSinePhaseOffset;
  if (phase >= 1.0f) {
    phase -= 1.0f;
  }
  // phase is explicitly wrapped above, so avoid Sine()'s redundant wrap in
  return SineNoWrap(phase) * kBraidsSineAmplitude + kBraidsSineDcOffset;
}

// digital_oscillator.cc:1029, Interpolate824(lut_svf_cutoff, cutoff << 16):
inline int32_t SvfCutoffCoefficient(float cutoff) {
  CONSTRAIN(cutoff, 0.0f, 32767.0f);
  const int cutoff_integer = static_cast<int>(cutoff);
  const int integral = cutoff_integer >> 8;
  const int fraction = cutoff_integer & 0xff;
  const int32_t a = kStruckDrumSvfCutoffTable[integral];
  const int32_t b = kStruckDrumSvfCutoffTable[integral + 1];
  // `a + ((b - a) * (fraction << 8) >> 16)`, i.e. a truncating interpolation.
  return a + ((b - a) * fraction >> 8);
}

// digital_oscillator.cc:1050-1052, one stage of the noise lowpass:
inline int32_t NoisePoleStep(
    int32_t state,
    int32_t input,
    int32_t coefficient) {
  return state + ((input - state) * coefficient >> 15);
}

}  // namespace

void StruckDrum::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void StruckDrum::Reset() {
  for (size_t i = 0; i < kNumStruckDrumPartials; ++i) {
    amplitude_[i] = 0.0f;
    phase_[i] = 0.0f;
  }
  lp0_[0] = lp0_[1] = 0.0f;
  lp1_[0] = lp1_[1] = 0.0f;
  lp2_[0] = lp2_[1] = 0.0f;
  ever_struck_ = false;
}

void StruckDrum::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  // The decay in this engine IS the note's whole loudness envelope,
  *already_enveloped = true;

  const bool stereo = false;

  const bool unpatched = (parameters.trigger & TRIGGER_UNPATCHED) != 0;
  const bool rising_edge = (parameters.trigger & TRIGGER_RISING_EDGE) != 0;
  // TRIGGER_UNPATCHED has no Braids equivalent; this follows the sibling
  const bool self_strike = unpatched && !ever_struck_;
  const bool strike = rising_edge || self_strike;

  if (strike) {
    // digital_oscillator.cc:995, a re-strike only resets phase if the
    const bool reset_phase = amplitude_[0] < kStruckDrumResetPhaseThreshold;
    for (size_t i = 0; i < kNumStruckDrumPartials; ++i) {
      // Braids' RAW (un-normalised) scale -- see THE AMPLITUDE STATE IS
      amplitude_[i] = static_cast<float>(kDrumPartialAmplitude[i]);
      if (reset_phase) {
        phase_[i] = kStruckDrumStrikePhase;
      }
    }
    ever_struck_ = true;
  } else if (!unpatched) {
    // digital_oscillator.cc:994-1015: a strike sets the target amplitude
    float timbre_clamped = parameters.timbre;
    CONSTRAIN(timbre_clamped, 0.0f, 1.0f);
    const float timbre16 = floorf(timbre_clamped * 32767.0f);
    if (timbre16 < kStruckDrumDroneThreshold) {
      // digital_oscillator.cc:1008-1009, reproduced with Braids' exact
      const float balance_raw = floorf((32767.0f - timbre16) / 256.0f);
      const float balance = floorf((balance_raw * balance_raw) / 128.0f);

      // MACRO's Decay spread -- see THE FOURTH MACRO in the header. Copied
      const float spread = ApplyMacro(
          kStruckDrumStockDecaySpread, kStruckDrumMinDecaySpread,
          kStruckDrumMaxDecaySpread, 0.5f);

      float block_decay[kNumStruckDrumPartials];
      float mean_decay = 0.0f;
      for (size_t i = 0; i < kNumStruckDrumPartials; ++i) {
        const float decay_long = kDrumPartialDecayLong[i];
        const float decay_short = kDrumPartialDecayShort[i];
        // digital_oscillator.cc:1010.
        const float decay16 = decay_long -
            floorf(((decay_long - decay_short) * balance) / 128.0f);
        block_decay[i] = decay16 / 65536.0f;
        mean_decay += block_decay[i];
      }
      mean_decay /= static_cast<float>(kNumStruckDrumPartials);

      // One Braids call is always 250 us of wall-clock time
      const size_t block_samples =
          static_cast<size_t>(kStruckDrumBlockSamples);
      size_t steps = (size + block_samples / 2) / block_samples;
      if (steps < 1) {
        steps = 1;
      }

      for (size_t i = 0; i < kNumStruckDrumPartials; ++i) {
        float adjusted = mean_decay + (block_decay[i] - mean_decay) * spread;
        CONSTRAIN(adjusted, 0.0f, 1.0f);
        for (size_t n = 0; n < steps; ++n) {
          // digital_oscillator.cc:1011-1012, `amp[i] * decay >> 16`
          amplitude_[i] = floorf(amplitude_[i] * adjusted + 5e-4f);
        }
      }
    }
  }

  // COLOR: noise cutoff, harmonics gain and the noise-mode crossfade, all
  float harmonics_clamped = parameters.harmonics;
  CONSTRAIN(harmonics_clamped, 0.0f, 1.0f);
  const float color16 = floorf(harmonics_clamped * 32767.0f);

  // digital_oscillator.cc:1023, `(pitch_ - 12*128) + (parameter_[1] >> 2)`.
  const float pitch128 =
      (parameters.note + kStruckDrumPitchCorrection) * 128.0f;
  const float color_shifted = floorf(color16 / 4.0f);  // >> 2
  const float cutoff = pitch128 - 12.0f * 128.0f + color_shifted;
  const int32_t noise_coefficient = SvfCutoffCoefficient(cutoff);

  // digital_oscillator.cc:1033.
  const float harmonics_gain = (color16 < kStruckDrumHarmonicsGainThreshold
      ? (color16 + kStruckDrumHarmonicsGainFloor)
      : 16384.0f) / 16384.0f;

  // digital_oscillator.cc:1034-1035. Computed in double: the intermediate
  const double noise_mode_gain_scaled =
      std::max(0.0, static_cast<double>(color16) - 16384.0) *
      static_cast<double>(kStruckDrumNoiseModeGainScale) / 16384.0;
  const float noise_mode_gain =
      static_cast<float>(std::floor(noise_mode_gain_scaled));
  const float noise_mode_gain_norm = noise_mode_gain / 16384.0f;

  // Per-channel mix weights (see the header's MIX WEIGHTS and OUT/AUX
  float mix1[2] = { 0.0f, 0.0f };
  float mix2[2] = { 0.0f, 0.0f };
  {
    float gain_norm = noise_mode_gain_norm -
        (stereo ? kStruckDrumStereoBlend : 0.0f);
    CONSTRAIN(gain_norm, 0.0f, 1.0f);
    mix1[0] = kStruckDrumNoiseModeMixConstant / 16384.0f - gain_norm;
    mix2[0] = gain_norm;
  }
  if (stereo) {
    float gain_norm = noise_mode_gain_norm + kStruckDrumStereoBlend;
    CONSTRAIN(gain_norm, 0.0f, 1.0f);
    mix1[1] = kStruckDrumNoiseModeMixConstant / 16384.0f - gain_norm;
    mix2[1] = gain_norm;
  }

  // MORPH: Spread, scaling all six fixed partial ratios together -- see THE
  const float morph_u = 2.0f * parameters.morph - 1.0f;
  const float spread_scale = morph_cubic_
      ? 1.0f + morph_u * morph_u * morph_u
      : parameters.morph * 2.0f;
  float increment[kNumStruckDrumPartials];
  for (size_t i = 0; i < kNumStruckDrumPartials; ++i) {
    const float ratio_semitones =
        (kDrumPartials[i] / 128.0f) * spread_scale;
    increment[i] = NoteToFrequency(parameters.note + ratio_semitones);
  }

  const int num_channels = stereo ? 2 : 1;

  for (size_t s = 0; s < size; ++s) {
    // The noise-lowpass chain: independent per channel in stereo (R14, see
    for (int channel = 0; channel < num_channels; ++channel) {
      // Braids' raw integer scale throughout (digital_oscillator.cc:1043-1052),
      int32_t noise = Random::GetSample();
      CONSTRAIN(noise, -16384, 16384);
      lp0_[channel] = NoisePoleStep(lp0_[channel], noise, noise_coefficient);
      lp1_[channel] = NoisePoleStep(lp1_[channel], lp0_[channel],
                                    noise_coefficient);
      lp2_[channel] = NoisePoleStep(lp2_[channel], lp1_[channel],
                                    noise_coefficient);
    }

    // The six tonal partials -- shared between channels, only the noise
    float partial[kNumStruckDrumPartials];
    float harmonics_sum = 0.0f;
    for (size_t i = 0; i < kNumStruckDrumPartials; ++i) {
      float sample_increment = increment[i];
      phase_[i] += sample_increment;
      if (phase_[i] >= 1.0f) {
        phase_[i] -= 1.0f;
      }
      // /65536.0f: amplitude_ is carried in Braids' raw integer-truncated
      partial[i] = BraidsSine(phase_[i]) * (amplitude_[i] / 65536.0f);
      harmonics_sum += partial[i];
    }

    // digital_oscillator.cc:1065-1070: partials[0] direct, the two
    const float lp2_left = lp2_[0] / 32768.0f;
    const float noise_mode_1 = partial[1] * lp2_left * 128.0f;
    const float noise_mode_2 = partial[3] * lp2_left * 64.0f;
    const float noise_term_l = noise_mode_1 * mix1[0] + noise_mode_2 * mix2[0];

    float out_sample = partial[0] + noise_term_l + harmonics_sum * harmonics_gain;
    // digital_oscillator.cc:1071, Braids' CLIP to +-32767 -- CONSTRAIN to
    CONSTRAIN(out_sample, -1.0f, 1.0f);
    *out++ = out_sample;

    if (stereo) {
      const float lp2_right = lp2_[1] / 32768.0f;
      const float noise_mode_1_r = partial[1] * lp2_right * 128.0f;
      const float noise_mode_2_r = partial[3] * lp2_right * 64.0f;
      const float noise_term_r =
          noise_mode_1_r * mix1[1] + noise_mode_2_r * mix2[1];
      float aux_sample =
          partial[0] + noise_term_r + harmonics_sum * harmonics_gain;
      CONSTRAIN(aux_sample, -1.0f, 1.0f);
      *aux++ = aux_sample;
    } else {
      // Mono AUX: the noise-ring-modulated component alone -- see the
      float aux_sample = noise_term_l;
      CONSTRAIN(aux_sample, -1.0f, 1.0f);
      *aux++ = aux_sample;
    }
  }
}

}  // namespace plaits
