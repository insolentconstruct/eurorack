// Copyright 2012 Emilie Gillet.
// Copyright 2021 Emilie Gillet.
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
// Wave Paraphonic: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_WAVE_PARAPHONIC_ENGINE_H_
#define BEADS_BLOB_MODELS_WAVE_PARAPHONIC_ENGINE_H_

#include "stmlib/dsp/hysteresis_quantizer.h"
#include "stmlib/dsp/units.h"
#include "plaits/dsp/engine/engine.h"

#ifndef PLAITS_BLOB
#include "plaits/resources.h"
#endif

namespace plaits {

const int kWaveParaphonicNumVoices = 4;

const int kWaveParaphonicNumSlots = 33;
const int kWaveParaphonicStep = 1024;

const float kWaveParaphonicMaxFan = 0.15f;

const float kWaveParaphonicMinSpread = 0.0f;
const float kWaveParaphonicMaxSpread = 2.0f;

// Palette converts integer cents before the ratio lookup.
class WaveParaphonicChordBank {
 public:
  void Init(stmlib::BufferAllocator* allocator) {
    chord_index_ = -1;
  }

  void Reset() {
    chord_index_ = -1;
    quantizer_.Init(11, 0.075f, false);
    set_chord(0.0f);
  }

  void set_chord(float parameter);

  inline float ratio(int voice) const { return ratios_[voice]; }

 private:
  stmlib::HysteresisQuantizer2 quantizer_;
  int chord_index_;
  float ratios_[4];
};

class WaveParaphonic {
 public:
  WaveParaphonic() { }
  ~WaveParaphonic() { }

  void Init(const int16_t* waves, stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  const int16_t* waves_;
  WaveParaphonicChordBank chords_;
  float phase_[kWaveParaphonicNumVoices];
  float frequency_[kWaveParaphonicNumVoices];

  bool strike_;

  DISALLOW_COPY_AND_ASSIGN(WaveParaphonic);
};

#ifndef PLAITS_BLOB
class WaveParaphonicEngine : public Engine {
 public:
  WaveParaphonicEngine() { }
  ~WaveParaphonicEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    core_.Init(wav_integrated_waves, allocator);
  }

  virtual void Reset() {
    core_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    core_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  WaveParaphonic core_;

  DISALLOW_COPY_AND_ASSIGN(WaveParaphonicEngine);
};
#endif

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_WAVE_PARAPHONIC_ENGINE_H_
