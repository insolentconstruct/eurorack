// Copyright 2012 Emilie Gillet.
// Copyright 2021 Emilie Gillet.
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
// Wave Paraphonic: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/wave_paraphonic_engine.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/utils/random.h"

#include "plaits/resources.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

inline float FastLog2(float x) {
  union { float f; uint32_t i; } u = { x };
  const float e = static_cast<float>((u.i >> 23) & 0xFFu) - 127.0f;
  u.i = (u.i & 0x007FFFFFu) | 0x3F800000u;
  const float m = u.f;
  return e + (-1.7417939f + (2.8212026f + (-1.4699568f
             + (0.44717955f - 0.056570851f * m) * m) * m) * m);
}

const uint8_t kWaveIndex[kWaveParaphonicNumSlots] = {
  182,  41,  57, 182, 145, 146, 122, 147,  57, 117, 151,
  167, 166, 165, 163, 162,  96, 129,  90, 130, 170, 132,
  134, 136, 138, 141, 140,  60,  62, 173, 121, 147, 178
};

const uint8_t kWaveGain[kWaveParaphonicNumSlots] = {
  121, 121, 130, 166, 118, 120, 100, 125, 147, 137, 218,
  121, 111, 127, 124, 121, 104, 103,  98, 104, 122, 141,
  126, 108, 112, 119, 106, 122, 125, 120,  94, 151, 127
};

const int kWaveTableSize = 128;

const float kIntegratedScale = 1.0f / 1024.0f;

inline const int16_t* WaveAt(const int16_t* waves, int slot) {
  return waves + kWaveIndex[slot] * (kWaveTableSize + 4);
}

inline float WaveGainAt(int slot) {
  return float(kWaveGain[slot]) / 128.0f;
}

struct WaveTap {
  const int16_t* low;
  const int16_t* high;
  float crossfade;
  float gain_low;
  float gain_high;
};

inline void ResolveTap(const int16_t* waves, int ticks, WaveTap* tap) {
  CONSTRAIN(ticks, 0, 32767);
  const int slot = ticks / kWaveParaphonicStep;
  const float crossfade = float(ticks - slot * kWaveParaphonicStep) /
      float(kWaveParaphonicStep);
  tap->low = WaveAt(waves, slot);
  tap->high = WaveAt(waves, slot + 1);
  tap->crossfade = crossfade;
  tap->gain_low = WaveGainAt(slot);
  tap->gain_high = WaveGainAt(slot + 1);
}

inline float ReadWave(const WaveTap& tap, float phase) {
  const float p = phase * float(kWaveTableSize);
  MAKE_INTEGRAL_FRACTIONAL(p);
  const float low_0 = float(tap.low[p_integral + 1] - tap.low[p_integral]);
  const float low_1 = float(tap.low[p_integral + 2] - tap.low[p_integral + 1]);
  const float high_0 = float(tap.high[p_integral + 1] - tap.high[p_integral]);
  const float high_1 =
      float(tap.high[p_integral + 2] - tap.high[p_integral + 1]);
  const float a = (low_0 + (low_1 - low_0) * p_fractional) * tap.gain_low;
  const float b = (high_0 + (high_1 - high_0) * p_fractional) * tap.gain_high;
  return (a + (b - a) * tap.crossfade) * kIntegratedScale;
}

}  // namespace

void WaveParaphonicChordBank::set_chord(float parameter) {
  static const int16_t cents[11][4] = {
    { 0, 1, 1199, 1200 }, { 0, 700, 701, 1200 },
    { 0, 500, 700, 1200 }, { 0, 300, 700, 1200 },
    { 0, 300, 700, 1000 }, { 0, 300, 1000, 1400 },
    { 0, 300, 1000, 1700 }, { 0, 200, 900, 1600 },
    { 0, 400, 1100, 1400 }, { 0, 400, 700, 1100 },
    { 0, 400, 700, 1200 }
  };
  quantizer_.Process(parameter * 1.02f);
  const int index = quantizer_.quantized_value();
  if (chord_index_ != index) {
    chord_index_ = index;
    for (int i = 0; i < 4; ++i) {
      ratios_[i] = SemitonesToRatio(static_cast<float>(cents[index][i]) * 0.01f);
    }
  }
}

void WaveParaphonic::Init(
    const int16_t* waves, BufferAllocator* allocator) {
  waves_ = waves;
  chords_.Init(allocator);
  Reset();
}

void WaveParaphonic::Reset() {
  chords_.Reset();
  for (int i = 0; i < kWaveParaphonicNumVoices; ++i) {
    phase_[i] = 0.0f;
    frequency_[i] = 0.01f;
  }
  strike_ = true;
}

void WaveParaphonic::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    strike_ = true;
  }

  chords_.set_chord(parameters.harmonics);

  const int timbre = static_cast<int>(parameters.timbre * 32767.0f);
  const int timbre_ticks = timbre < 0 ? 0 : (timbre > 32767 ? 32767 : timbre);
  const int fan = static_cast<int>((parameters.morph - 0.5f) * 2.0f *
      kWaveParaphonicMaxFan * 32767.0f);

  const float spread = 1.0f;

  WaveTap tap[kWaveParaphonicNumVoices];
  float target_frequency[kWaveParaphonicNumVoices];

  for (int i = 0; i < kWaveParaphonicNumVoices; ++i) {
    ResolveTap(waves_, timbre_ticks + i * fan, &tap[i]);

    const float interval = 12.0f * FastLog2(chords_.ratio(i));
    const float note = parameters.note + interval * spread;
    target_frequency[i] = NoteToFrequency(note);
  }

  if (strike_) {
    for (int i = 0; i < kWaveParaphonicNumVoices; ++i) {
      phase_[i] = static_cast<float>(Random::GetWord()) / 4294967296.0f;
      frequency_[i] = target_frequency[i];
    }
    strike_ = false;
  }

  ParameterInterpolator fm_0(&frequency_[0], target_frequency[0], size);
  ParameterInterpolator fm_1(&frequency_[1], target_frequency[1], size);
  ParameterInterpolator fm_2(&frequency_[2], target_frequency[2], size);
  ParameterInterpolator fm_3(&frequency_[3], target_frequency[3], size);
  ParameterInterpolator* fm[kWaveParaphonicNumVoices] = {
    &fm_0, &fm_1, &fm_2, &fm_3
  };

  while (size--) {
    float sum = 0.0f;
    float first_voice = 0.0f;

    for (int i = 0; i < kWaveParaphonicNumVoices; ++i) {
      float increment = fm[i]->Next();
      float phase = phase_[i] + increment;
      if (phase >= 1.0f) {
        phase -= 1.0f;
      }
      phase_[i] = phase;

      const float sample = ReadWave(tap[i], phase);

      sum += sample;
      if (i == 0) {
        first_voice = sample;
      }
    }

    float main = sum * 0.25f;
    float second = first_voice * 0.5f;
    CONSTRAIN(main, -1.0f, 1.0f);
    CONSTRAIN(second, -1.0f, 1.0f);

    *out++ = main;
    *aux++ = second;
  }
}

}  // namespace plaits
