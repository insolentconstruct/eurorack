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

#ifndef BEADS_BLOB_MODELS_BUZZ_ENGINE_H_
#define BEADS_BLOB_MODELS_BUZZ_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const int kBuzzNumZones = 15;

const float kBuzzZoneSemitones = 8.0f;

const float kBuzzShiftSemitones = 32767.0f / 256.0f;

const float kBuzzZoneSelectMaxNote = 128.0f;

const float kBuzzStockDetuneSemitones = 32767.0f / 32768.0f;

const float kBuzzMacroMaxDetuneSemitones = 12.0f;

const float kBuzzMaxSpreadZones = 4.0f;

class Buzz {
 public:
  Buzz() { }
  ~Buzz() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  float phase0_;
  float phase1_;
  float frequency0_;
  float frequency1_;
  float zone_position0_;
  float zone_position1_;

  float osc0_decimator_;
  float osc1_decimator_;

  DISALLOW_COPY_AND_ASSIGN(Buzz);
};

class BuzzEngine : public Engine {
 public:
  BuzzEngine() { }
  ~BuzzEngine() { }

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
  Buzz engine_;

  DISALLOW_COPY_AND_ASSIGN(BuzzEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_BUZZ_ENGINE_H_
