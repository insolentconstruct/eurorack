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
// Cymbal: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_CYMBAL_ENGINE_H_
#define BEADS_BLOB_MODELS_CYMBAL_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const float kCymbalRootOffsetSemitones = 40.0f;

const float kCymbalPartialRatio1 = 24273.0f / 16384.0f;
const float kCymbalPartialRatio2 = 12561.0f / 16384.0f;
const float kCymbalPartialRatio3 = 18417.0f / 16384.0f;
const float kCymbalPartialRatio4 = 22452.0f / 16384.0f;
const float kCymbalPartialRatio5 = 31858.0f / 16384.0f;

const float kCymbalNoiseClockMultiplier = 24.0f;

const float kCymbalMetallicScale = 5461.0f / 32768.0f;

const float kCymbalToneIndexRange = 32767.0f / 256.0f;

const float kCymbalCutoffCapFraction = 1.0f / 8.0f;

const float kCymbalOversampledRate = 96000.0f;

const float kCymbalDampBandpassStock = 14751.0f / 32768.0f;
const float kCymbalDampHighpassStock = 33089.0f / 32768.0f;

const float kCymbalSpreadMax = 2.0f;

class Cymbal {
 public:
  Cymbal() : morph_cubic_(true) { }
  ~Cymbal() { }

  void set_morph_cubic(bool cubic) { morph_cubic_ = cubic; }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  float partial_phase_[6];

  float bp_lp_;
  float bp_bp_;

  float noise_clock_[2];
  uint32_t rng_state_[2];
  float hp_lp_[2];
  float hp_bp_[2];

  float xfade_;

  bool morph_cubic_;

  DISALLOW_COPY_AND_ASSIGN(Cymbal);
};

class CymbalEngine : public Engine {
 public:
  CymbalEngine() { }
  ~CymbalEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    cymbal_.Init(allocator);
  }

  virtual void Reset() {
    cymbal_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    cymbal_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Cymbal cymbal_;

  DISALLOW_COPY_AND_ASSIGN(CymbalEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_CYMBAL_ENGINE_H_
