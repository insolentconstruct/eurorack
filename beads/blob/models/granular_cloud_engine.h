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
// Granular Cloud: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_GRANULAR_CLOUD_ENGINE_H_
#define BEADS_BLOB_MODELS_GRANULAR_CLOUD_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const int kGranularCloudNumGrains = 4;

const int kGranularCloudSchedulerPeriod = 12;

const float kGranularCloudSpawnStock = 16384.0f;
const float kGranularCloudSpawnMin = 1024.0f;
const float kGranularCloudSpawnMax = 65536.0f;

const float kGranularCloudRateSemitonesPerStep = 12.0f / 64.0f;
const float kGranularCloudRateBase = 2048.0f;

const uint32_t kGranularCloudEnvelopeEnd = 1 << 24;
const int kGranularCloudRateShift = 4;

const float kGranularCloudPeakLate = 0.9f;
const float kGranularCloudPeakEarly = 0.1f;

const float kGranularCloudGrainGain = 0.25f;

const float kGranularCloudStereoSpread = 0.8f;

const uint32_t kGranularCloudSeed = 0x21;

class GranularCloud {
 public:
  GranularCloud() { }
  ~GranularCloud() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);


 private:
  void ScheduleGrains(int timbre_code, int color_code, uint32_t spawn_threshold,
      float base_increment);
  inline uint32_t NextWord();

  struct Grain {
    float phase;
    float phase_increment;
    uint32_t envelope_phase;
    uint32_t envelope_phase_increment;
  };

  Grain grain_[kGranularCloudNumGrains];

  float aux_phase_;
  float peak_position_;

  int scheduler_countdown_;
  uint32_t rng_state_;

  DISALLOW_COPY_AND_ASSIGN(GranularCloud);
};

class GranularCloudEngine : public Engine {
 public:
  GranularCloudEngine() { }
  ~GranularCloudEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    granular_cloud_.Init(allocator);
  }

  virtual void Reset() {
    granular_cloud_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    granular_cloud_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  GranularCloud granular_cloud_;

  DISALLOW_COPY_AND_ASSIGN(GranularCloudEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_GRANULAR_CLOUD_ENGINE_H_
