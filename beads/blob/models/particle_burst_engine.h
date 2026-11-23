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
// ParticleBurst: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_PARTICLE_BURST_ENGINE_H_
#define BEADS_BLOB_MODELS_PARTICLE_BURST_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {

// The three filters' fixed intervals above the note, in semitones.
const float kParticleFilter1Interval = 12.0f;
const float kParticleFilter2Interval = 19.0f;
const float kParticleFilter3Interval = 15.125f;

// Jitter divisors preserve Braids' distinct per-filter pitch shifts.
const float kParticleJitter1Divisor = 131072.0f;  // >> 17, filter 1 (x3 first)
const float kParticleJitter2Divisor = 32768.0f;   // >> 15, filter 2
const float kParticleJitter3Divisor = 65536.0f;   // >> 16, filter 3

// Braids truncates the squared resonance to this Q15 value.
const float kParticleResonanceSquared = 32506.0f / 32768.0f;

// Braids' fixed decay is neutral at the fourth macro's midpoint.
const float kParticleStockDecay = 64763.0f / 65536.0f;
const float kParticleMinDecay = 0.90f;
const float kParticleMaxDecay = 0.9995f;

// Match the module pitch correction for a table-indexed resonator.
const float kParticlePitchCorrection = 0.046105f;

// Braids compares a 23-bit draw against density plus a fixed floor.
const uint32_t kParticleDensityMask = 0x7fffffu;
const float kParticleDensityFloor = 1024.0f;
const float kParticleDensityRange = 32767.0f;

class ParticleBurst {
 public:
  ParticleBurst() { }
  ~ParticleBurst() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  // Retain the source layout; mono uses channel zero.
  float amplitude_[2];
  float y1_state_[2][2];  // [channel][y[n-1], y[n-2]]
  float y2_state_[2][2];
  float y3_state_[2][2];
  float c1_[2];
  float c2_[2];
  float c3_[2];
  float s1_[2];
  float s2_[2];
  float s3_[2];

  DISALLOW_COPY_AND_ASSIGN(ParticleBurst);
};

class ParticleBurstEngine : public Engine {
 public:
  ParticleBurstEngine() { }
  ~ParticleBurstEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    particle_burst_.Init(allocator);
  }

  virtual void Reset() {
    particle_burst_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    particle_burst_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  ParticleBurst particle_burst_;

  DISALLOW_COPY_AND_ASSIGN(ParticleBurstEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_PARTICLE_BURST_ENGINE_H_
