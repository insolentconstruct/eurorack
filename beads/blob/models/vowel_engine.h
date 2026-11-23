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
//
// Vowel: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_VOWEL_ENGINE_H_
#define BEADS_BLOB_MODELS_VOWEL_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const int kVowelNumFormants = 3;

// The port's inner loop runs at Braids' own 96 kHz.
const int kVowelOversampling = 2;

// The 31-tap halfband decimator's logical history, rounded up to a power of
const int kVowelFirMask = 31;
const int kVowelFirSize = 32;
const int kVowelFirBufferSize = kVowelFirSize * 2;

// digital_oscillator.cc:478, `consonant_frames = 160`, decremented once per
const int kVowelConsonantSamples = 160 * 24;

// digital_oscillator.cc:475. COLOR spans 200..711, a 3.555x multiplier.
const int kVowelMinFormantShift = 200;

// Braids' real pitch ceiling for this shape is NOT `kHighestNote = 140 * 128`
const float kVowelHighestNote = 127.9921875f;

// SPEC R4: every increment is bounded so that, expressed at the 48 kHz output
const uint32_t kVowelMaxFormantIncrement = 536870912u;  // 0.125 * 2^32

// Spread scales formants 2 and 3 against formant 1. Half to double is enough
const float kVowelMinSpread = 0.5f;
const float kVowelMaxSpread = 2.0f;

// Grain is the length of the per-period ramp as a multiple of the period.
const float kVowelMinGrain = 0.2f;
const float kVowelMaxGrain = 3.0f;

// Opposing formant shifts for the two stereo channels. Small: the formant
const float kVowelStereoDetune = 0.02f;

class Vowel {
 public:
  Vowel() { }
  ~Vowel() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);


 private:
  int NextRandomSample();

  uint32_t phase_;
  uint32_t phase_increment_;
  uint32_t formant_phase_[kVowelNumFormants];
  uint32_t formant_phase_r_[kVowelNumFormants];

  // Braids' per-block frame state: state_.vow.formant_increment /
  uint32_t formant_increment_[kVowelNumFormants];
  int formant_amplitude_[kVowelNumFormants];
  int noise_;
  int consonant_samples_;
  bool strike_pending_;

  // stmlib::Random seeded as the module seeds it (random.cc:34).
  uint32_t rng_state_;

  float fir_a_[kVowelFirBufferSize];
  float fir_b_[kVowelFirBufferSize];
  int fir_write_;

  float dc_input_;
  float dc_output_;
  float dc_input_aux_;
  float dc_output_aux_;

  DISALLOW_COPY_AND_ASSIGN(Vowel);
};

class VowelEngine : public Engine {
 public:
  VowelEngine() { }
  ~VowelEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    vowel_.Init(allocator);
  }

  virtual void Reset() {
    vowel_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    vowel_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Vowel vowel_;

  DISALLOW_COPY_AND_ASSIGN(VowelEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_VOWEL_ENGINE_H_
