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

#ifndef BEADS_BLOB_MODELS_STRUCK_DRUM_ENGINE_H_
#define BEADS_BLOB_MODELS_STRUCK_DRUM_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const size_t kNumStruckDrumPartials = 6;

// digital_oscillator.cc:841-843. Partial ratios, in Braids' 1/128-semitone
const int16_t kDrumPartials[kNumStruckDrumPartials] = {
  0, 0, 1041, 1747, 1846, 3072
};

// digital_oscillator.cc:845-846. Initial per-partial strike amplitude, in
const int16_t kDrumPartialAmplitude[kNumStruckDrumPartials] = {
  16986, 2654, 3981, 5308, 3981, 2985
};

// digital_oscillator.cc:849-850 and :853-854. Per-partial per-BLOCK decay
const uint16_t kDrumPartialDecayLong[kNumStruckDrumPartials] = {
  65533, 65531, 65531, 65531, 65531, 65516
};
const uint16_t kDrumPartialDecayShort[kNumStruckDrumPartials] = {
  65083, 64715, 64715, 64715, 64715, 62312
};

// digital_oscillator.cc:1004, the literal threshold on Braids' 0..32767
const float kStruckDrumDroneThreshold = 32000.0f;

// digital_oscillator.cc:995, `partial_amplitude[0] < 1024` -- the threshold
const float kStruckDrumResetPhaseThreshold = 1024.0f;

// digital_oscillator.cc:998-999, `(1L << 30)` of a 32-bit phase accumulator
const float kStruckDrumStrikePhase = 0.25f;

// 12 * log2(kSampleRate / kCorrectedSampleRate) -- +4.61 cents on the module,
const float kStruckDrumPitchCorrection = 0.046105f;

// One Braids call to RenderStruckDrum is ALWAYS kBlockSize = 24 samples at
const float kStruckDrumBlockSamples = 12.0f;

// braids/resources.cc:114-224, entries 0-128 of lut_svf_cutoff (see TABLE
const uint16_t kStruckDrumSvfCutoffTable[129] = {
     17,    18,    19,    20,    22,    23,    24,    26,
     27,    29,    31,    33,    35,    37,    39,    41,
     44,    46,    49,    52,    55,    58,    62,    66,
     70,    74,    78,    83,    88,    93,    99,   105,
    111,   117,   124,   132,   140,   148,   157,   166,
    176,   187,   198,   210,   222,   235,   249,   264,
    280,   297,   314,   333,   353,   374,   396,   420,
    445,   471,   499,   529,   561,   594,   629,   667,
    706,   748,   793,   840,   890,   943,   999,  1059,
   1122,  1188,  1259,  1334,  1413,  1497,  1586,  1681,
   1781,  1886,  1999,  2117,  2243,  2377,  2518,  2668,
   2826,  2994,  3172,  3361,  3560,  3772,  3996,  4233,
   4485,  4751,  5033,  5332,  5648,  5983,  6337,  6713,
   7111,  7532,  7978,  8449,  8949,  9477, 10037, 10628,
  11254, 11916, 12616, 13356, 14138, 14964, 15837, 16758,
  17730, 18756, 19837, 20975, 22174, 23435, 24761, 25078,
  25078
};

// digital_oscillator.cc:1033. Q14 gain floor/ceiling on the six-partial
const float kStruckDrumHarmonicsGainThreshold = 12888.0f;
const float kStruckDrumHarmonicsGainFloor = 4096.0f;

// digital_oscillator.cc:1034-1035,1068-1069. See the header's MIX WEIGHTS
const float kStruckDrumNoiseModeGainScale = 12888.0f;
const float kStruckDrumNoiseModeMixConstant = 12288.0f;

// MACRO's Decay spread range -- copied verbatim from struck_bell_engine.h's
const float kStruckDrumMinDecaySpread = 0.0f;
const float kStruckDrumStockDecaySpread = 1.0f;
const float kStruckDrumMaxDecaySpread = 4.0f;

// Stereo noise-mode crossfade offset (see the OUT/AUX note above) --
const float kStruckDrumStereoBlend = 0.15f;

class StruckDrum {
 public:
  StruckDrum() : morph_cubic_(true) { }
  ~StruckDrum() { }

  // MORPH's response: true (the default since 2026-10-05, chosen by ear on
  void set_morph_cubic(bool cubic) { morph_cubic_ = cubic; }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);


 private:
  // Shared between channels -- only the noise-lowpass chain differs L/R
  float amplitude_[kNumStruckDrumPartials];
  float phase_[kNumStruckDrumPartials];

  // [0] = mono/left (always runs), [1] = right (stereo only, its own
  int32_t lp0_[2];
  int32_t lp1_[2];
  int32_t lp2_[2];

  bool ever_struck_;

  bool morph_cubic_;

  DISALLOW_COPY_AND_ASSIGN(StruckDrum);
};

class StruckDrumEngine : public Engine {
 public:
  StruckDrumEngine() { }
  ~StruckDrumEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    struck_drum_.Init(allocator);
  }

  virtual void Reset() {
    struck_drum_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    struck_drum_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  StruckDrum struck_drum_;

  DISALLOW_COPY_AND_ASSIGN(StruckDrumEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_STRUCK_DRUM_ENGINE_H_
