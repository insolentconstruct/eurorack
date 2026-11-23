// Copyright 2026 Dylan Bolink.
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
// Clap: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_CLAP_ENGINE_H_
#define BEADS_BLOB_MODELS_CLAP_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/filter.h"
#include "stmlib/utils/random.h"

namespace plaits {

const float kClapStockSlaps = 4;
const float kClapHandDecline = 0.93f;
const int kClapMaxSlaps = 7;
const float kClapTailLevel = 0.75f;
const float kClapLastGapRatio = 0.78f;

const float kClapHighpassQ1 = 0.5411961f;
const float kClapHighpassQ2 = 1.3065630f;

const float kClapLowpassQ = 0.7071068f;

// Tone changes bandwidth around a fixed geometric centre.
const float kClapBandCenterRatio = 1.2806f;
const float kClapHalfWidthMin = 0.155f;
const float kClapHalfWidthStock = 0.6432f;
const float kClapHalfWidthMax = 1.355f;

// AUX in mono is the same band with its top an octave up.
const float kClapLowpassOctavesAux = 2.0f;
const int kClapIdle = 0;

class Clap {
 public:
  Clap() { }
  ~Clap() { }
  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters, float* out, float* aux,
      size_t size, bool* already_enveloped);

 private:
  inline void Tick() {
    if (countdown_ != kClapIdle) {
      --countdown_;
      if (countdown_ == kClapIdle) {
        FireSlap();
      }
    }
    slap_env_ *= slap_decay_;
    tail_env_ *= tail_decay_;
  }

  void FireSlap();

  // The band, as one call: two highpass stages then one lowpass pole.
  inline float Band(float x) {
    x = highpass_[0].Process<stmlib::FILTER_MODE_HIGH_PASS>(x);
    x = highpass_[1].Process<stmlib::FILTER_MODE_HIGH_PASS>(x);
    return lowpass_.Process<stmlib::FILTER_MODE_LOW_PASS>(x);
  }

  inline float BandAux(float x) {
    x = highpass_aux_[0].Process<stmlib::FILTER_MODE_HIGH_PASS>(x);
    x = highpass_aux_[1].Process<stmlib::FILTER_MODE_HIGH_PASS>(x);
    return lowpass_aux_.Process<stmlib::FILTER_MODE_LOW_PASS>(x);
  }

  // The room opens.
  inline void FireTail() {
    const float tail = kClapTailLevel * slap_level_;
    if (tail > tail_env_) {
      tail_env_ = tail;
    }
  }

  // Schedules the first slap of a burst, on the next Tick.
  inline void ArmBurst(int slaps) {
    slaps_remaining_ = slaps;
    slap_index_ = 0;
    burst_gain_ = 1.0f;
    countdown_ = 1;
  }

  int spacing_samples_;
  float jitter_;
  float pan_spread_;
  float slap_level_;
  float last_slap_gain_;
  int num_slaps_;
  bool free_run_;
  int free_run_gap_;

  float slap_decay_;
  float tail_decay_;

  float prev_makeup_;
  float prev_makeup_aux_;

  int countdown_;
  int slaps_remaining_;

  float burst_gain_;
  int slap_index_;
  float slap_env_;
  float tail_env_;

  float slap_gain_l_;
  float slap_gain_r_;

  stmlib::Svf highpass_[2];
  stmlib::Svf lowpass_;

  stmlib::Svf highpass_aux_[2];
  stmlib::Svf lowpass_aux_;

  DISALLOW_COPY_AND_ASSIGN(Clap);
};

class ClapEngine : public Engine {
 public:
  ClapEngine() { }
  ~ClapEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    clap_.Init(allocator);
  }

  virtual void Reset() {
    clap_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    clap_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Clap clap_;

  DISALLOW_COPY_AND_ASSIGN(ClapEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_CLAP_ENGINE_H_
