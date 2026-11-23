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
// RawFm: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_RAW_FM_ENGINE_H_
#define BEADS_BLOB_MODELS_RAW_FM_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {
const float kRawFmCentreIndex = 1.0f;
const float kRawFmEdgeIndex = 0.5f;
const float kRawFmPhaseFeedback = 0.125f;
const float kRawFmFrequencyFeedback = 0.25f;
const float kRawFmPhaseOffset = 4.75f;
const float kRawFmWtfmCentre = 129.0f / 256.0f;
const float kRawFmWtfmFeedbackSteps = 64.0f;
const float kRawFmAttenuatorPivotNote = 72.0f;
const float kRawFmAttenuatorSlope = 512.0f;
const float kRawFmAttenuatorFull = 32767.0f / 32768.0f;
const float kRawFmModulatorNoteCeiling = 128.0f - 1.0f / 128.0f - 12.0f;

class RawFm {
 public:
  RawFm() { }
  ~RawFm() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  float phase_;
  float modulator_phase_;
  float previous_sample_;

  DISALLOW_COPY_AND_ASSIGN(RawFm);
};

class RawFmEngine : public Engine {
 public:
  RawFmEngine() { }
  ~RawFmEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    raw_fm_.Init(allocator);
  }

  virtual void Reset() {
    raw_fm_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    raw_fm_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  RawFm raw_fm_;

  DISALLOW_COPY_AND_ASSIGN(RawFmEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_RAW_FM_ENGINE_H_
