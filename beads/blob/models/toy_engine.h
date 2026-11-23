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

// Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_TOY_ENGINE_H_
#define BEADS_BLOB_MODELS_TOY_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const float kToyMaxDecimation = 256.0f;

const float kToyStereoClockRatio = 1.0293f;

class Toy {
 public:
  Toy() { }
  ~Toy() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  float phase_;
  float ramp_increment_;

  float decimation_counter_;
  uint8_t held_sample_;

  float decimation_counter_r_;
  uint8_t held_sample_r_;

  float downsampler_state_;
  float downsampler_state_r_;

  float dc_input_;
  float dc_output_;
  float dc_input_aux_;
  float dc_output_aux_;

  DISALLOW_COPY_AND_ASSIGN(Toy);
};

class ToyEngine : public Engine {
 public:
  ToyEngine() { }
  ~ToyEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    engine_.Init(allocator);
  }

  virtual void Reset() {
    engine_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    engine_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Toy engine_;

  DISALLOW_COPY_AND_ASSIGN(ToyEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_TOY_ENGINE_H_
