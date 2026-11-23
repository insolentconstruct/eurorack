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
// SawSquare: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_SAW_SQUARE_ENGINE_H_
#define BEADS_BLOB_MODELS_SAW_SQUARE_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const float kSawSquareSawPwMin = 1024.0f / 65536.0f;

const float kSawSquareSquarePwMin = 768.0f / 65536.0f;

const float kSawSquareAttenuationStock = 148.0f / 256.0f;

const float kSawSquareMaxPhaseOffset = 1.0f;

class SawSquare {
 public:
  SawSquare() { }
  ~SawSquare() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:

  float phase_;
  float frequency_;
  float pw_saw_;
  float previous_pw_saw_;
  bool high_saw_;
  float next_sample_saw_;

  float phase_square_;
  float pw_square_;
  float previous_pw_square_;
  bool high_square_;
  float next_sample_square_;

  float offset_;

  float atten_;

  float blend_;

  DISALLOW_COPY_AND_ASSIGN(SawSquare);
};

class SawSquareEngine : public Engine {
 public:
  SawSquareEngine() { }
  ~SawSquareEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    saw_square_.Init(allocator);
  }

  virtual void Reset() {
    saw_square_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    saw_square_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  SawSquare saw_square_;

  DISALLOW_COPY_AND_ASSIGN(SawSquareEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_SAW_SQUARE_ENGINE_H_
