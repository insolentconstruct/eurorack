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

#ifndef BEADS_BLOB_MODELS_STRUCK_BELL_ENGINE_H_
#define BEADS_BLOB_MODELS_STRUCK_BELL_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const size_t kNumBellPartials = 11;

const int16_t kBellPartials[kNumBellPartials] = {
  -1284, -1283, -184, -183, 385, 1175, 1536, 2233, 2434, 2934, 3110
};

const int16_t kBellPartialAmplitudes[kNumBellPartials] = {
  8192, 5488, 8192, 14745, 21872, 13680, 11960, 10895, 10895, 6144, 10895
};

const uint16_t kBellPartialDecayLong[kNumBellPartials] = {
  65533, 65533, 65533, 65532, 65531, 65531, 65530, 65529, 65527, 65523, 65519
};
const uint16_t kBellPartialDecayShort[kNumBellPartials] = {
  65308, 65283, 65186, 65123, 64839, 64889, 64632, 64409, 64038, 63302, 62575
};

const float kBellDroneThreshold = 32000.0f;

const float kBellBlockSamples = 12.0f;

const float kBellOutputScale = 0.25f;

const float kBellMaxDetuneSemitones = 1.9921875f;

const float kBellMinDecaySpread = 0.0f;
const float kBellStockDecaySpread = 1.0f;
const float kBellMaxDecaySpread = 4.0f;

const float kBellBrightnessDepth = 1.5f;

class StruckBell {
 public:
  StruckBell() { }
  ~StruckBell() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  int32_t amplitude_[kNumBellPartials];
  float phase_sin_[kNumBellPartials];
  float phase_cos_[kNumBellPartials];

  bool ever_struck_;

  DISALLOW_COPY_AND_ASSIGN(StruckBell);
};

class StruckBellEngine : public Engine {
 public:
  StruckBellEngine() { }
  ~StruckBellEngine() { }

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
  StruckBell engine_;

  DISALLOW_COPY_AND_ASSIGN(StruckBellEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_STRUCK_BELL_ENGINE_H_
