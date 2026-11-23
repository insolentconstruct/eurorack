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
// Undertow: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_UNDERTOW_ENGINE_H_
#define BEADS_BLOB_MODELS_UNDERTOW_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const int kNumUndertowVoices = 6;

class Undertow {
 public:
  Undertow() { }
  ~Undertow() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  void ComputeRegistration(float registration, float* amplitude) const;

  float phase_[kNumUndertowVoices];
  float frequency_[kNumUndertowVoices];
  float main_amplitude_[kNumUndertowVoices];
  float aux_amplitude_[kNumUndertowVoices];
  float next_blep_[kNumUndertowVoices];
  bool pulse_high_[kNumUndertowVoices];
  float colour_;
  float pulse_width_[kNumUndertowVoices];
  bool pulse_mode_;

  DISALLOW_COPY_AND_ASSIGN(Undertow);
};

class UndertowEngine : public Engine {
 public:
  UndertowEngine() { }
  ~UndertowEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    undertow_.Init(allocator);
  }

  virtual void Reset() {
    undertow_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    undertow_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Undertow undertow_;

  DISALLOW_COPY_AND_ASSIGN(UndertowEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_UNDERTOW_ENGINE_H_
