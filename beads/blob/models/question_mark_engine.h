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
// QuestionMark: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_QUESTION_MARK_ENGINE_H_
#define BEADS_BLOB_MODELS_QUESTION_MARK_ENGINE_H_

#include "plaits/dsp/engine/engine.h"

namespace plaits {
const int32_t kQuestionMarkDitBase = 3600;
const int32_t kQuestionMarkNoiseFloorBase = 1024;
const int32_t kQuestionMarkSeedAtStrike = 32767;
const int16_t kQuestionMarkLeadInTicks = 10;
const int16_t kQuestionMarkTerminatorTicks = 100;
const int32_t kQuestionMarkMaxIntensity = 16000;
const float kQuestionMarkHighestNote = 127.9921875f;
const uint32_t kQuestionMarkRngSeed = 0x21;
const uint32_t kQuestionMarkRngSeedAux = 0x9e3779b9;
const float kQuestionMarkMaxBed = 2.5f;
const float kQuestionMarkMaxGrit = 2.0f;
const float kQuestionMarkDcCoefficient = 0.0002f;

class QuestionMark {
 public:
  QuestionMark() { }
  ~QuestionMark() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  void Strike();
  uint32_t phase_;
  uint32_t tick_phase_;       // state->cycle_phase
  uint32_t symbol_index_;     // state->cycle_phase_increment
  uint32_t key_;              // state->rng_state
  int32_t seed_;              // state->seed
  int16_t ticks_left_;        // state->sample

  uint32_t rng_state_;
  uint32_t rng_state_aux_;

  float dc_state_;
  float dc_state_aux_;

  DISALLOW_COPY_AND_ASSIGN(QuestionMark);
};

class QuestionMarkEngine : public Engine {
 public:
  QuestionMarkEngine() { }
  ~QuestionMarkEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    question_mark_.Init(allocator);
  }

  virtual void Reset() {
    question_mark_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    question_mark_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  QuestionMark question_mark_;

  DISALLOW_COPY_AND_ASSIGN(QuestionMarkEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_QUESTION_MARK_ENGINE_H_
