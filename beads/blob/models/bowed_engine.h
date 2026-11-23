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

#ifndef BEADS_BLOB_MODELS_BOWED_ENGINE_H_
#define BEADS_BLOB_MODELS_BOWED_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const size_t kBowedBridgeLength = 1024;
const size_t kBowedNeckLength = 4096;

const float kBowedBridgeLpGain = 14008.0f / 32768.0f;
const float kBowedBridgeLpPole = 18022.0f / 32768.0f;

const float kBowedBodyGain = 6553.0f / 32768.0f;
const float kBowedBodyRadius = 0.849949f;
const float kBowedStockTheta = 0.0657752f;

const float kBowedPressureMin = 45.0f;
const float kBowedPressureMax = 172.0f;

const float kBowedBowPositionMin = 6.0f / 256.0f;
const float kBowedBowPositionMax = 69.0f / 256.0f;

const float kBowedEnvelopePeak = 6553.0f / 32768.0f;
const float kBowedEnvelopeSustain = 5242.0f / 32768.0f;
const float kBowedEnvelopeRise = 599.0f;
const float kBowedEnvelopeHold = 720.0f;

const float kBowedTilt = 0.15f;

const float kBowedExciterDcPole = 0.999f;

const float kBowedExciterGain = 6.77f;

class Bowed {
 public:
  Bowed() { }
  ~Bowed() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  int8_t* bridge_line_;
  int8_t* neck_line_;

  uint32_t delay_pointer_;
  float excitation_;
  float bridge_lp_;

  float body_y0_;
  float body_y1_;
  float body_aux_y0_;
  float body_aux_y1_;

  float tilt_state_;
  float tilt_state_aux_;

  float exciter_dc_in_;
  float exciter_dc_out_;

  DISALLOW_COPY_AND_ASSIGN(Bowed);
};

class BowedEngine : public Engine {
 public:
  BowedEngine() { }
  ~BowedEngine() { }

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
  Bowed engine_;

  DISALLOW_COPY_AND_ASSIGN(BowedEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_BOWED_ENGINE_H_
