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
// Fold: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_FOLD_ENGINE_H_
#define BEADS_BLOB_MODELS_FOLD_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const float kFoldMinDepth = 2048.0f / 32768.0f;
const float kFoldDepthRange = 30720.0f / 32768.0f;

const float kFoldTriGuardNote = 80.0f;
const float kFoldTriGuardSpan = 32767.0f / (7.0f * 128.0f);
const float kFoldSineGuardNote = 92.0f;
const float kFoldSineGuardSpan = 32767.0f / (6.0f * 128.0f);

const float kFoldMaxSymmetry = 0.5f;

class Fold {
 public:
  Fold() { }
  ~Fold() { }

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

  float depth_;
  float blend_;
  float symmetry_;
  float drive_;

  float downsampler_state_;
  float downsampler_state_aux_;

  float dc_input_;
  float dc_input_aux_;

  float* shaper_;

  DISALLOW_COPY_AND_ASSIGN(Fold);
};

class FoldEngine : public Engine {
 public:
  FoldEngine() { }
  ~FoldEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    fold_.Init(allocator);
  }

  virtual void Reset() {
    fold_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    fold_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Fold fold_;

  DISALLOW_COPY_AND_ASSIGN(FoldEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_FOLD_ENGINE_H_
