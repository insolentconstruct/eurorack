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
// Snare: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_SNARE_ENGINE_H_
#define BEADS_BLOB_MODELS_SNARE_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {
const float kSnareSvfLutSampleRate = 96000.0f;
const float kSnareSvfMaxNormalizedFrequency = 0.125f;

const float kSnareClip = 32767.0f / 32768.0f;

const int32_t kSnarePulse0DelayTicks = 0;
const float kSnarePulse0Decay = 1536.0f / 4096.0f;
const int32_t kSnarePulse1DelayTicks = 48;
const float kSnarePulse1Decay = 3072.0f / 4096.0f;
const int32_t kSnarePulse2DelayTicks = 48;
const float kSnarePulse2Decay = 1200.0f / 4096.0f;
const int32_t kSnarePulse3DelayTicks = 0;

const float kSnarePulse0Level = 15.0f * 32768.0f / 32768.0f;
const float kSnarePulse1Level = -1.0f * 32768.0f / 32768.0f;
const float kSnarePulse1Hold = 2621.0f / 32768.0f;
const float kSnarePulse2Level = 13107.0f / 32768.0f;
const float kSnarePulse2Hold = 13107.0f / 32768.0f;

const float kSnareSnappyCeiling = 14336.0f;

const float kSnarePulse3LevelBase = 512.0f / 32768.0f;
const float kSnarePulse3LevelSlope = 2.0f / 32768.0f;

const float kSnareDecayPitchBase = 49152.0f;
const float kSnareColorDecayThreshold = 16384.0f;
const float kSnareDecayMax = 65535.0f;
const float kSnareTone0ResonanceBase = 29000.0f;
const float kSnareTone1ResonanceBase = 26500.0f;
const float kSnareResonanceDecayScale = 1.0f / 32.0f;
const float kSnarePulse3DecayBase = 4092.0f;
const float kSnarePulse3DecayScale = 1.0f / 16384.0f;
const float kSnarePulse3DecayNormalize = 1.0f / 4096.0f;

const float kSnareNoiseResonanceCode = 2000.0f;

const float kSnareTone0SemitoneOffset = 12.0f;
const float kSnareTone1SemitoneOffset = 24.0f;
const float kSnareNoiseSemitoneOffset = 60.0f;
const float kSnareMinNote = 0.0f;
const float kSnareMaxNote = 140.0f;

const float kSnareBalanceBase = 22000.0f / 32768.0f;
const float kSnareBalanceSpan = 16383.5f / 32768.0f;

const float kSnareDryMix = 1.0f / 16.0f;

class Snare {
 public:
  Snare() { }
  ~Snare() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  bool ever_struck_;

  // Pulse 3 snapshots its decay and level on each strike.
  float pulse0_state_;
  int32_t pulse0_counter_;
  float pulse0_level_;
  float pulse1_state_;
  int32_t pulse1_counter_;
  float pulse1_level_;
  float pulse2_state_;
  int32_t pulse2_counter_;
  float pulse2_level_;
  float pulse3_state_;
  int32_t pulse3_counter_;
  float pulse3_level_;
  float pulse3_decay_;

  // The three resonators retain their low-pass and band-pass states.
  float svf0_lp_;
  float svf0_bp_;
  float svf1_lp_;
  float svf1_bp_;
  float svf2_lp_;
  float svf2_bp_;

  // Tonal resonances are fixed between strikes.
  float tone0_resonance_domain_;
  float tone1_resonance_domain_;

  float gain0_;
  float gain1_;
  float tone_mix_gain_;
  float noise_mix_gain_;
  float partial_spread_;

  DISALLOW_COPY_AND_ASSIGN(Snare);
};

class SnareEngine : public Engine {
 public:
  SnareEngine() { }
  ~SnareEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    snare_.Init(allocator);
  }

  virtual void Reset() {
    snare_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    snare_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Snare snare_;

  DISALLOW_COPY_AND_ASSIGN(SnareEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_SNARE_ENGINE_H_
