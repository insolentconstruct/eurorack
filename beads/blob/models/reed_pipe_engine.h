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
// Reed Pipe: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_REED_PIPE_ENGINE_H_
#define BEADS_BLOB_MODELS_REED_PIPE_ENGINE_H_

#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/physical_modelling/delay_line.h"

namespace plaits {

const size_t kReedPipeDelaySize = 2048;

// Rest opening of the reed, held clear of both stops so that the static
// breath pressure can never park the reed shut.
const float kReedPipeRestOpening = 0.40f;

// Stiffness acts on the oscillating part of the pressure difference only, so
// it can span a wide range without moving the reed's operating point.
const float kReedPipeStiffnessMin = 2.0f;
const float kReedPipeStiffnessRange = 12.0f;

// Reflection-filter corner, in harmonics of the played note. Tracking pitch
// keeps MACRO, MORPH and TIMBRE responsive across the whole keyboard; with a
// fixed coefficient they collapsed on low notes.
const float kReedPipeBrightnessMin = 1.0f;
const float kReedPipeBrightnessRange = 12.0f;

class ReedPipe {
 public:
  ReedPipe() { }
  ~ReedPipe() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  DelayLine<float, kReedPipeDelaySize> bore_;

  float delay_;
  float return_filter_;
  float outgoing_;
  float breath_envelope_;
  float excitation_;
  float reflection_coefficient_;
  float reflection_brightness_;
  float reed_stiffness_;
  float breath_noise_;
  float pickup_position_;
  float pickup_amount_;
  float out_dc_input_;
  float out_dc_output_;
  float aux_dc_input_;
  float aux_dc_output_;
  uint32_t noise_state_;

  DISALLOW_COPY_AND_ASSIGN(ReedPipe);
};

class ReedPipeEngine : public Engine {
 public:
  ReedPipeEngine() { }
  ~ReedPipeEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    reed_pipe_.Init(allocator);
  }

  virtual void Reset() {
    reed_pipe_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    reed_pipe_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  ReedPipe reed_pipe_;

  DISALLOW_COPY_AND_ASSIGN(ReedPipeEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_REED_PIPE_ENGINE_H_
