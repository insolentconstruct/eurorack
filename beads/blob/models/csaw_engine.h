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
// CSaw: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_CSAW_ENGINE_H_
#define BEADS_BLOB_MODELS_CSAW_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const float kCSawMaxPulseWidth = 0.375f;

const float kCSawDepthOffset = -0.125f;
const float kCSawDepthRange = 0.5f;

const float kCSawDcShift = 2047.0f / 32768.0f;
const float kCSawMakeUp = 1.625f;

const float kCSawPulseStep = 2.0f;

const float kCSawPulseGain = 0.70f;

class CSaw {
 public:
  CSaw() { }
  ~CSaw() { }

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
  float pw_;
  float bend_;
  float tilt_;
  float previous_pw_;
  bool high_;

  float depth_;

  float next_sample_;
  float next_sample_aux_;

  DISALLOW_COPY_AND_ASSIGN(CSaw);
};

class CSawEngine : public Engine {
 public:
  CSawEngine() { }
  ~CSawEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    csaw_.Init(allocator);
  }

  virtual void Reset() {
    csaw_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    csaw_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  CSaw csaw_;

  DISALLOW_COPY_AND_ASSIGN(CSawEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_CSAW_ENGINE_H_
