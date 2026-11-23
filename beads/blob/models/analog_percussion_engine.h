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
// AnalogPercussion: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_ANALOG_PERCUSSION_ENGINE_H_
#define BEADS_BLOB_MODELS_ANALOG_PERCUSSION_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/filter.h"
#include "stmlib/utils/random.h"

namespace plaits {

const float kPercussionStockCycles = 30.0f;
const float kPercussionMinCycles = 4.0f;
const float kPercussionMaxCycles = 200.0f;

const float kPercussionDecibelSpan = 4.6f;

const float kPercussionStockRatio = 3.57f;
const float kPercussionMinRatio = 1.0f;
const float kPercussionMaxRatio = 12.0f;

const float kPercussionOvertoneMax = 10.0f;
const float kPercussionShapeStart = 0.55f;
const float kPercussionOvertoneCowbell = 1.5f;
const float kPercussionCowbellRatio = 1.481f;

const float kPercussionBandRatio = 1.10f;
const float kPercussionShapeBias = 0.0f;

const float kPercussionFastTauMs = 29.0f;
const float kPercussionFastLevel = 1.0f;

const float kPercussionSlowDuck = 0.45f;
const float kPercussionVcaDrive = 1.0f;
const float kPercussionShapeDrive = 9.0f;

const float kPercussionShapeFullHz = 900.0f;
const float kPercussionShapeSilentHz = 2400.0f;

const float kPercussionBandQ = 1.4f;

const float kPercussionClickCycles = 2.5f;

// A highpass retains the rimshot's broadband click.
const float kPercussionClickRatio = 2.0f;
const float kPercussionClickQ = 0.7f;

// Retain the original stereo constants and state layout.
const float kPercussionOvertonePan = 0.75f;
const float kPercussionClickPan = 0.25f;
const float kPercussionFundamentalPan = 0.30f;
const float kPercussionTwinPan = 0.70f;
const float kPercussionCentre = 0.707106781f;

const float kPercussionStereoDetuneCents = 12.0f;
const float kPercussionTwinPhaseOffset = 0.25f;

// Trim the stereo pair to retain the mono power.
const float kPercussionTwinTrim = 0.707106781f;

const float kPercussionSubRatio = 0.5f;
const float kPercussionClickDroneTrim = 0.18f;

const int kPercussionIdle = 0;

class AnalogPercussion {
 public:
  AnalogPercussion() { }
  ~AnalogPercussion() { }
  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters, float* out, float* aux,
      size_t size, bool* already_enveloped);

 private:
  inline float Shape(float x) {
    if (shape_wave_ <= 0.0f) {
      return x;
    }
    const float shaped =
        (stmlib::SoftClip(x * shape_drive_ + shape_bias_) - shape_dc_) *
        shape_norm_;
    return x + (shaped - x) * shape_wave_;
  }

  float phase_[2];
  float phase_increment_[2];
  float mode_decay_[2];
  float mode_env_[2];

  float shape_amount_;
  float shape_wave_;
  float shape_drive_;
  float shape_bias_;
  float shape_dc_;
  float shape_norm_;

  stmlib::Svf shape_band_;
  stmlib::Svf shape_band_aux_;

  // Stereo only: a detuned copy of the fundamental, sharing its envelope.
  float twin_phase_;
  float twin_increment_;

  // AUX is an enveloped octave-down strike sharing the trigger.
  float sub_phase_;
  float sub_increment_;
  float sub_decay_;
  float sub_env_;

  float fast_env_;
  float fast_decay_;

  float click_env_;
  float click_decay_;
  float click_level_;

  // Held across the block so a knob step lands as a ramp rather than an edge.
  float prev_gain_;
  float prev_gain_aux_;

  // An unpatched trigger sustains the modes at a constant level.
  float sustain_gain_;

  stmlib::Svf click_filter_;

  DISALLOW_COPY_AND_ASSIGN(AnalogPercussion);
};

class AnalogPercussionEngine : public Engine {
 public:
  AnalogPercussionEngine() { }
  ~AnalogPercussionEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    analog_percussion_.Init(allocator);
  }

  virtual void Reset() {
    analog_percussion_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    analog_percussion_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  AnalogPercussion analog_percussion_;

  DISALLOW_COPY_AND_ASSIGN(AnalogPercussionEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_ANALOG_PERCUSSION_ENGINE_H_
