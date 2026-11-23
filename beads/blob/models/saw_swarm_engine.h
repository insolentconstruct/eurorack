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
// SawSwarm: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_SAW_SWARM_ENGINE_H_
#define BEADS_BLOB_MODELS_SAW_SWARM_ENGINE_H_

#include "stmlib/dsp/filter.h"

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const int kNumSawSwarmVoices = 7;

// digital_oscillator.cc:169: detune = ((parameter_[0]+1024)^2)>>9, then
const float kSawSwarmDetuneScale = 1.0f / 4096.0f;

// digital_oscillator.cc:187-196: the pivot is COLOR code 10922 (not exactly
const float kSawSwarmColorPivot = 10922.0f / 32767.0f;
const float kSawSwarmColorSlopeBelow = 24.0f * 32767.0f / 4096.0f;
const float kSawSwarmColorSlopeAbove = 12.0f * 32767.0f / 4096.0f;

// The filter cutoff table is generated over MIDI notes 0..256
const float kSawSwarmCutoffNoteMax = 256.0f;

// The generator's `fc <= fs/8` clamp (lookup_tables.py:100) at Braids' native
const float kSawSwarmCutoffHzMax = 12000.0f;

// lut_svf_damp[0] (resources.cc:182) = 65534, applied via Braids' `>>15`
const float kSawSwarmDampStock = 65534.0f / 32768.0f;
const float kSawSwarmResonanceStock = 1.0f / kSawSwarmDampStock;

// MACRO's Resonance range around that stock value: calmer (more damped, no
const float kSawSwarmResonanceCalm = 0.35f;
const float kSawSwarmResonancePeak = 3.0f;

// Sum-gain from digital_oscillator.cc:220-227: seven +-1 naive sawtooths
const float kSawSwarmSumGain = 4096.0f / 32768.0f;

// tanh(2.0): the shaper's own peak before the generator's `scale()`
const float kSawSwarmShaperPeak = 0.9640275800758169f;

class SawSwarm {
 public:
  SawSwarm() { }
  ~SawSwarm() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  float phase_[kNumSawSwarmVoices];
  float frequency_[kNumSawSwarmVoices];

  float cutoff_frequency_;
  float resonance_;
  float morph_;

  stmlib::Svf svf_;

  DISALLOW_COPY_AND_ASSIGN(SawSwarm);
};

class SawSwarmEngine : public Engine {
 public:
  SawSwarmEngine() { }
  ~SawSwarmEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    saw_swarm_.Init(allocator);
    saw_swarm_.Reset();
  }

  virtual void Reset() {
    saw_swarm_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    saw_swarm_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  SawSwarm saw_swarm_;

  DISALLOW_COPY_AND_ASSIGN(SawSwarmEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_SAW_SWARM_ENGINE_H_
