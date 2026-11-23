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
// Harmonics: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_HARMONICS_ENGINE_H_
#define BEADS_BLOB_MODELS_HARMONICS_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const int kHarmonicsNumPartials = 12;

const int kHarmonicsPositionScale = 256;

const float kHarmonicsSineAmplitude = 32639.0f / 32768.0f;

const float kHarmonicsIncrementScale = 65536.0f;
const int32_t kHarmonicsGuardThreshold = 0x4000;

const float kHarmonicsMaxSpread = 6.0f;

const float kHarmonicsMinWidthScale = 1.0f / 16.0f;
const float kHarmonicsMaxWidthScale = 16.0f;

const float kHarmonicsStereoPeak = 0.12f;

class Harmonics {
 public:
  Harmonics() { }
  ~Harmonics() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  void ComputeTargets(
      float peak_position,
      float colour,
      float spread,
      float width_scale,
      int32_t increment_msb,
      int32_t* target);

  float phase_;
  float frequency_;

  int32_t amplitude_[kHarmonicsNumPartials];
  int32_t amplitude_aux_[kHarmonicsNumPartials];

  DISALLOW_COPY_AND_ASSIGN(Harmonics);
};

class HarmonicsEngine : public Engine {
 public:
  HarmonicsEngine() { }
  ~HarmonicsEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    harmonics_.Init(allocator);
  }

  virtual void Reset() {
    harmonics_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    harmonics_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Harmonics harmonics_;

  DISALLOW_COPY_AND_ASSIGN(HarmonicsEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_HARMONICS_ENGINE_H_
