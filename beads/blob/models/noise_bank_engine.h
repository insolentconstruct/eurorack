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
// NoiseBank: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_NOISE_BANK_ENGINE_H_
#define BEADS_BLOB_MODELS_NOISE_BANK_ENGINE_H_

#include "stmlib/dsp/hysteresis_quantizer.h"

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const float kNoiseBankMaxNote = 140.0f;

const float kNoiseBankDampIndexScale = 1.0f / 128.0f;
const float kNoiseBankResonanceDivisor = 260.0f;

const float kNoiseBankMaxCutoff = 0.125f;

const int kNoiseBankClockShifts = 3;

enum NoiseBankModel {
  NOISE_BANK_MODEL_FILTERED,
  NOISE_BANK_MODEL_TWIN_PEAKS,
  NOISE_BANK_MODEL_CLOCKED,
  NOISE_BANK_MODEL_LAST
};

const size_t kNoiseBankDecimatorSize = 32;
const uint32_t kNoiseBankDecimatorMask = 31;

class NoiseBank {
 public:
  NoiseBank() { }
  ~NoiseBank() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  inline void Push(float main, float side) {
    decimator_main_[decimator_write_ & kNoiseBankDecimatorMask] = main;
    decimator_aux_[decimator_write_ & kNoiseBankDecimatorMask] = side;
    ++decimator_write_;
  }

  float lp_;
  float bp_;

  int32_t y11_;
  int32_t y12_;
  int32_t y21_;
  int32_t y22_;
  float twin_main_;
  float twin_aux_;

  uint32_t clock_phase_;
  uint32_t cycle_phase_;
  uint32_t rng_state_;
  uint32_t rng_state_aux_;
  uint32_t seed_;
  uint32_t seed_aux_;
  int32_t clocked_sample_;
  int32_t clocked_sample_aux_;

  float drive_;

  float decimator_main_[kNoiseBankDecimatorSize];
  float decimator_aux_[kNoiseBankDecimatorSize];
  uint32_t decimator_write_;

  stmlib::HysteresisQuantizer2 model_quantizer_;

  DISALLOW_COPY_AND_ASSIGN(NoiseBank);
};

class NoiseBankEngine : public Engine {
 public:
  NoiseBankEngine() { }
  ~NoiseBankEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    noise_bank_.Init(allocator);
  }

  virtual void Reset() {
    noise_bank_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    noise_bank_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  NoiseBank noise_bank_;

  DISALLOW_COPY_AND_ASSIGN(NoiseBankEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_NOISE_BANK_ENGINE_H_
