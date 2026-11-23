// Copyright 2012 Olivier Gillet.
// Copyright 2015 Tim Churches.
// Copyright 2026 Lyle Mills.
// SPDX-License-Identifier: MIT
// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
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
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Palette port: mono, no FM input, fourth macro fixed at 0.5.

#ifndef BEADS_BLOB_MODELS_BYTEBEAT_ENGINE_H_
#define BEADS_BLOB_MODELS_BYTEBEAT_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const int kBytebeatNumFormulas = 4;

const float kBytebeatTicksPerCycle = 256.0f;

const uint32_t kBytebeatRestartTick = 1024;

const float kBytebeatAuxRatio = 0.5f;

const float kBytebeatDcPole = 0.999f;

class Bytebeat {
 public:
  Bytebeat() { }
  ~Bytebeat() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  float tick_phase_;
  float aux_tick_phase_;
  uint32_t t_;
  uint32_t aux_t_;
  float dc_in_;
  float dc_out_;
  float dc_aux_in_;
  float dc_aux_out_;

  DISALLOW_COPY_AND_ASSIGN(Bytebeat);
};

class BytebeatEngine : public Engine {
 public:
  BytebeatEngine() { }
  ~BytebeatEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    bytebeat_.Init(allocator);
  }

  virtual void Reset() {
    bytebeat_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    bytebeat_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Bytebeat bytebeat_;

  DISALLOW_COPY_AND_ASSIGN(BytebeatEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_BYTEBEAT_ENGINE_H_
