// Copyright 2026 Dylan Bolink.
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
// FreshetsFormant: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_FRESHETS_FORMANT_ENGINE_H_
#define BEADS_BLOB_MODELS_FRESHETS_FORMANT_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

#include "stmlib/dsp/filter.h"

namespace plaits {

class FreshetsFormant {
 public:
  FreshetsFormant() { }
  ~FreshetsFormant() { }
  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters, float* out, float* aux,
      size_t size, bool* already_enveloped);

 private:
  float driver_phase_;
  float env_phase_;
  float env_phase_r_;

  float next_sample_hollow_;
  float next_sample_sub_;
  bool sub_state_;
  int prev_hollow_state_;

  float prev_f0_;
  float prev_ratio_;
  float prev_pw_;
  float prev_shape_;

  float lp_1_;
  float lp_2_;
  float lp_1_r_;
  float lp_2_r_;

  stmlib::OnePole dc_blocker_[2];

  DISALLOW_COPY_AND_ASSIGN(FreshetsFormant);
};

class FreshetsFormantEngine : public Engine {
 public:
  FreshetsFormantEngine() { }
  ~FreshetsFormantEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    freshets_formant_.Init(allocator);
  }

  virtual void Reset() {
    freshets_formant_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    freshets_formant_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  FreshetsFormant freshets_formant_;

  DISALLOW_COPY_AND_ASSIGN(FreshetsFormantEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_FRESHETS_FORMANT_ENGINE_H_
