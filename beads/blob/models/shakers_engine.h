// Copyright 1995-2023 Perry R. Cook and Gary P. Scavone.
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
// Shakers: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_SHAKERS_ENGINE_H_
#define BEADS_BLOB_MODELS_SHAKERS_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

const int kShakersNumInstruments = 16;

// Angklung has seven tubes; mug-and-coins is four mug modes plus three coin
const int kShakersMaxResonances = 7;

// STK's constants were tuned at 44.1 kHz.
const float kShakersReferenceRate = 44100.0f;

// The note that plays an instrument at its measured frequencies.
const float kShakersReferenceNote = 60.0f;

// Below this the shake is over and the engine returns silence, as upstream.
const float kShakersMinEnergy = 1e-8f;

// Upstream's MAX_SHAKE.
const float kShakersMaxShake = 1.0f;

// A ratchet's energy ramp, per tooth.
const float kShakersGuiroRatchetDelta = 0.0001f;
const float kShakersWrenchRatchetDelta = 0.00015f;

// Water drop resonators sweep upward as each drop is born.
const float kShakersWaterFreqSweep = 1.0001f;

// LEVEL MATCHING, and it is the difference between an instrument selector and a
const float kShakersLevelTarget = 0.035f;

struct ShakerPreset {
  int num_resonances;
  uint16_t frequencies_offset;
  uint16_t radii_offset;
  uint16_t gains_offset;
  float num_objects;
  float system_decay;
  float sound_decay;
  float gain;
  float decay_scale;
  float vary_factor;
  // Which resonances jitter their centre frequency on each collision. A bit
  uint8_t vary_mask;
  // Equalizer numerator, applied to the summed resonator output.
  float eq_b0;
  float eq_b1;
  float eq_b2;
  // 0 shake, 1 ratchet, 2 water.
  uint8_t mechanism;
  float ratchet_delta;
  // Measured level-matching gain. See the note on kShakersLevelTarget.
  float makeup;
};

extern const ShakerPreset kShakerPresets[kShakersNumInstruments];

class Shakers {
 public:
  Shakers() { }
  ~Shakers() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);
  // Pattern A: the resonated instrument against the bare collisions.

 private:
  void SetInstrument(int instrument);

  int instrument_;
  float shake_energy_;
  float sound_level_;
  float current_gain_;
  float num_objects_;
  float system_decay_;

  // Per-resonance biquad state. Two poles, no zeros -- the equalizer supplies
  float a1_[kShakersMaxResonances];
  float a2_[kShakersMaxResonances];
  float y1_[kShakersMaxResonances];
  float y2_[kShakersMaxResonances];
  float gain_[kShakersMaxResonances];
  float normalization_[kShakersMaxResonances];
  float frequency_[kShakersMaxResonances];
  float radius_[kShakersMaxResonances];

  float eq_x1_;
  float eq_x2_;

  // Ratchet mechanism.
  float ratchet_count_;
  float ratchet_delta_;

  // Water mechanism.
  float water_frequency_[kShakersMaxResonances];

  float note_ratio_;

  DISALLOW_COPY_AND_ASSIGN(Shakers);
};

class ShakersEngine : public Engine {
 public:
  ShakersEngine() { }
  ~ShakersEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    shakers_.Init(allocator);
  }

  virtual void Reset() {
    shakers_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    shakers_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  Shakers shakers_;

  DISALLOW_COPY_AND_ASSIGN(ShakersEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_SHAKERS_ENGINE_H_
