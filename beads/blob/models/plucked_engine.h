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
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// Plucked: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_PLUCKED_ENGINE_H_
#define BEADS_BLOB_MODELS_PLUCKED_ENGINE_H_

#include "stmlib/dsp/hysteresis_quantizer.h"

#include "beads/blob/models/palette_chord_bank.h"

#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/physical_modelling/delay_line.h"

namespace plaits {

// digital_oscillator.h:49.
const int kNumPluckVoices = 3;

// Matches Braids' own per-voice buffer cap (digital_oscillator.cc:1105);
const size_t kPluckedDelaySize = 1024;
const float kPluckedMinPeriod = 4.0f;

// TIMBRE's damping/stretch split, digital_oscillator.cc:1125-1136. p0/p1
const float kPluckedParamFull = 32767.0f;
const float kPluckedTimbreHalf = 16384.0f;
const float kPluckedUpdateProbabilityFull = 65535.0f;
const float kPluckedUpdateProbabilityBase = 131072.0f;
const float kPluckedUpdateProbabilitySlope = 31.0f / 8.0f;
const float kPluckedProbabilityFull = 65536.0f;
const float kPluckedLossFull = 32768.0f;
const float kPluckedLossPitchBase = 4096.0f;
const float kPluckedLossPitchFloor = 256.0f;
// phase_increment_ >> 14 at Braids' post-<<=1, per-48kHz-sample scaling is
const float kPluckedLossFreqScale = 262144.0f;

// COLOR's excitation width, digital_oscillator.cc:1110-1112.
const float kPluckedWidthGain = 1.5f;
const float kPluckedWidthBase = 8192.0f;
const float kPluckedWidthFull = 65536.0f;

// MACRO: Stretch, scales how far below 1.0 the update-probability fraction
const float kPluckedMaxStretch = 2.5f;

// string_engine.cc:60, reused verbatim for the same round-robin-into-
extern const float kPluckedPan[kNumPluckVoices];

class Plucked {
 public:
  Plucked() { }
  ~Plucked() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  DelayLine<float, kPluckedDelaySize> delay_line_[kNumPluckVoices];
  float period_[kNumPluckVoices];
  float min_period_[kNumPluckVoices];
  float excitation_remaining_[kNumPluckVoices];

  // MORPH's chord: the knob's slot (0 = unison, then the table), refreshed
  int SelectChord(const EngineParameters& parameters, float* ratios);

  int active_voice_;
  bool ever_struck_;

  PaletteChordBank chords_;
  stmlib::HysteresisQuantizer2 slot_quantizer_;
  int slot_count_;
  int slot_;
  int tone_counter_;
  float active_ratio_;

  float loss_frac_;
  float update_probability_frac_;

  DISALLOW_COPY_AND_ASSIGN(Plucked);
};

class PluckedEngine : public Engine {
 public:
  PluckedEngine() { }
  ~PluckedEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    plucked_.Init(allocator);
  }

  virtual void Reset() {
    plucked_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    plucked_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Plucked plucked_;

  DISALLOW_COPY_AND_ASSIGN(PluckedEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_PLUCKED_ENGINE_H_
