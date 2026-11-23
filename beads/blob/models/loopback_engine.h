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
// Loopback: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_LOOPBACK_ENGINE_H_
#define BEADS_BLOB_MODELS_LOOPBACK_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const float kLoopbackFeedbackDepth = 0.5f;

class Loopback {
 public:
  Loopback() { }
  ~Loopback() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  float carrier_phase_;
  float feedback_phase_;
  float feedback_state_;
  float fb_osc_;
  float ratio_;
  float depth_;
  float morph_;
  float polarity_;

  DISALLOW_COPY_AND_ASSIGN(Loopback);
};

class LoopbackEngine : public Engine {
 public:
  LoopbackEngine() { }
  ~LoopbackEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    loopback_.Init(allocator);
  }

  virtual void Reset() {
    loopback_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    loopback_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Loopback loopback_;

  DISALLOW_COPY_AND_ASSIGN(LoopbackEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_LOOPBACK_ENGINE_H_
