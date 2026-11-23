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

#include "beads/blob/models/question_mark_engine.h"

#include "stmlib/dsp/dsp.h"

namespace plaits {

using namespace stmlib;

namespace {

inline float BedGain(float morph) {
  const float amount = morph * 2.0f;
  return morph < 0.5f
      ? amount
      : 1.0f + (kQuestionMarkMaxBed - 1.0f) * (amount - 1.0f);
}


const uint8_t kMorseCode[1064] = {
     5,    0,  132,    0,   20,   16,   20,   81,   16,   65,    8,   17,
     4,   65,   17,    5,    0,   69,    1,   88,   17,   25,    0,  132,
   144,    0,   64,   80,    0,   21,  148,    0,   65,   17,    5,  129,
     4,    1,   68,    1,   65,    5,   65,   17,   21,    4,   20,   16,
     4,  128,   80,    0,    4,   64,   20,   21,    0,    4,    1,   20,
    16,    9,   17,   68,   17,    5,   17,    4,    1,  132,    0,   65,
    16,   20,   81,  145,    1,   81,   17,   21,   16,   21,   81,    1,
    20,   16,   21,   68,   16,   16,  144,    5,    0,  132,   17,    4,
    65,   68,  129,   65,   20,   81,  129,    0,    4,   20,   65,  129,
    17,    5,   80,    5,   64,   64,    1,  132,   64,   65,   17,   68,
    65,   64,   17,    5,   80,    0,    4,    1,   20,   16,   25,   64,
     4,   17,    4,   20,   16,   84,   20,  128,    5,    0,  132,    0,
    65,   80,    1,   65,   24,   81,    0,  129,   80,  129,   17,    5,
    64,    1,   65,   64,   16,   64,   16,   64,   17,    5,   64,    1,
    65,   17,    5,   17,  132,    5,   80,    1,   64,   20,    1,   16,
    25,   81,    0,   80,    4,  129,   16,    5,    0,    4,   20,   16,
     4,    0,    8,   16,    4,    1,   20,   81,   16,   69,    1,    8,
    21,   72,    0,   80,    4,   65,    1,    0,   80,    1,   65,    0,
    64,   80,   65,   17,    4,   20,   20,  129,    1,    4,   21,   20,
    16,  132,   17,    5,   16,    8,    1,    4,    4,   64,    0,    4,
     5,   17,   21,   81,    0,    5,  128,    0,   65,   64,   17,    4,
    80,   16,   68,    0,    4,    1,    1,   89,   17,   21,   64,   80,
     1,  145,    0,   68,   20,   69,    1,   88,   17,   25,    0,  132,
    16,   65,   80,    1,   81,    1,    4,   21,   16,   21,    1,   16,
     9,   21,   20,  128,    4,    0,   69,   16,   20,   16,   21,   81,
    65,    8,   20,    4,   64,   64,    1,  132,   21,  145,    1,   64,
    24,   64,   16,    4,   80,   65,    1,    9,    5,    1,    4,   65,
     4,   21,   64,    1,   81,   16,   16,  144,   17,    4,    1,    4,
     5,   65,   20,  128,   80,    9,    5,    1,    4,    1,   88,    0,
    64,    8,   81,   80,    1,   81,   17,    0,  145,   17,   69,    1,
     4,    4,   17,    4,    9,   21,   20,  128,   81,   84,   17,   64,
    17,   68,   16,    8,   16,   20,   81,    0,   21,   20,  128,  144,
     5,   21,    0,    4,    1,  132,   64,  129,    1,   64,   80,    1,
    65,    1,    5,    1,    1,    9,    9,   81,   64,   17,   64,   20,
    68,    1,   24,    0,    4,    5,   65,   69,   65,    1,   68,   16,
     8,   81,    4,    5,   65,   64,   65,   17,    8,   64,    0,   64,
    80,    1,   68,    0,   24,    5,   85,    4,   65,   64,   80,   16,
    64,   64,   17,   64,   20,  128,   80,   65,    1,   24,   69,   21,
     1,   20,   65,    4,  129,   17,    5,   65,    1,   68,   16,   68,
     1,   24,    0,   20,   81,    0,    5,   65,    1,   64,   17,   21,
     4,    1,    1,   25,   81,   20,   64,   64,   16,   65,   80,   17,
     0,  145,    1,   65,    0,   64,   20,   16,   20,   80,   64,   65,
    17,   88,    0,   64,   24,    0,    4,    5,   65,   17,    0,  145,
    17,    4,   65,    4,  145,   65,    4,   65,    0,   80,   17,    5,
    80,    0,    4,    1,    1,   89,    0,   64,    4,   65,    8,   81,
    80,    0,   88,    0,   64,   64,    0,    8,   17,  133,   65,    8,
    64,   80,   64,    0,   24,    1,    5,   80,   17,    5,    8,   21,
     0,   20,   81,    0,  149,    5,    0,  132,    0,   20,   16,   20,
    81,   16,   65,   24,   16,    4,   65,   17,    5,   81,    1,   20,
    17,    0,   88,    0,   64,   20,   16,    9,    5,    1,    4,    1,
     8,   81,   17,    5,   65,   24,   65,   16,   64,   80,    0,    4,
    64,    4,  128,   80,   65,    1,    8,   64,    5,    5,   65,   20,
   128,   80,   25,   16,   21,   81,    0,   21,    1,   16,    9,   64,
    64,   16,   64,   20,   84,   16,   16,  144,   20,    0,   21,   16,
    68,   16,   65,    9,   16,   20,   81,   16,    8,   25,   16,   20,
    81,    0,    5,   17,    4,    1,   68,    1,    0,   80,    5,    0,
     4,   65,  132,   65,    4,    5,   65,    4,  129,    5,    0,  132,
     1,   20,   81,   17,    5,   65,   17,    0,  145,   16,    5,    0,
    20,  145,   16,   69,   16,  132,   20,   20,   65,   80,   17,   68,
     1,    8,   20,    8,   25,   20,   81,    0,   68,    1,    0,   80,
    16,   65,   64,    1,   65,    1,    5,   20,   20,  129,    1,   65,
    17,   21,   84,    4,   64,   21,    1,   16,    9,   64,   68,   64,
    65,   17,    8,    0,   20,   81,   16,    9,   16,    4,    5,  129,
     5,    0,   68,    1,  145,    1,   65,   17,    5,   80,   16,   64,
     1,    8,   16,    4,    1,    4,   20,   16,   20,  144,   64,    9,
    21,   16,    4,   65,   17,    5,   64,    0,   88,    0,   64,    8,
    65,   17,   21,   81,   81,   16,   16,  144,  144,    0,    4,   80,
     1,   20,   64,   20,   24,   16,    4,    0,   20,   81,   16,    4,
    80,    0,   24,   81,    0,  129,   16,    5,    0,   20,   81,   17,
     5,   17,    4,  128,   80,   65,    1,   24,   16,    5,   20,    0,
    20,    0,    4,    1,   68,    0,   24,    0,    4,   80,   16,    4,
    64,    9,   16,    4,   65,   17,   21,    9,   25,   80,   64,   65,
     1,   24,   81,    0,  129,   16,   81,    0,   21,   80,   24,    0,
    20,   81,    1,  144,   80,   89,    0,   64,    8,   16,    4,    5,
   129,   20,   20,  128,   17,    5,   16,   88,    0,   64,    8,   65,
    17,   21,   81,   81,   16,   16,  144,    4,    0,   69,   16,   20,
    16,   21,    0,   20,   81,    1,   20,   16,   25,    1,    5,   80,
    64,   89,   80,   16,   64,    1,    5,   20,   20,   65,   16,   16,
   144,    5,    0,  132,    1,   64,   80,   16,   84,   20,   20,   64,
     4,  129,    5,    4,   17,   84,   17,   69,    1,   24,    0,    4,
    21,   16,   20,   80,   17,    0,  145,   16,    5,   84,    0,  128,
     5,    0,  132,    1,    4,   65,   64,   65,    1,    5,   64,   16,
    16,  144,   16,    5,    0,    4,   85,   16,   17,   65,    0,    8,
     0,    4,    5,   17,    4,   17,   68,   65,   64,   65,   17,    4,
    16,    1,   24,   81,   20,   64,   64,   16,   65,  144,   16,    5,
     0,    4,    4,   64,   16,   65,    4,   65,   20,   64,   16,   16,
   144,    5,    0,    4,   85,   16,   17,   65,    0,   24,    0,   20,
    16,    9,   64,   21,   81,    1,   65,    1,    5,    0,    4,    5,
    80,    0,   68,   65,   16,   16,   80,  255
};

const int16_t kBraidsSine[257] = {
   -32512,  -32502,  -32473,  -32423,  -32356,  -32265,  -32160,  -32031,
   -31885,  -31719,  -31533,  -31331,  -31106,  -30864,  -30605,  -30324,
   -30028,  -29712,  -29379,  -29026,  -28658,  -28272,  -27868,  -27449,
   -27011,  -26558,  -26089,  -25604,  -25103,  -24588,  -24056,  -23512,
   -22953,  -22378,  -21793,  -21191,  -20579,  -19954,  -19316,  -18667,
   -18006,  -17334,  -16654,  -15960,  -15259,  -14548,  -13828,  -13100,
   -12363,  -11620,  -10868,  -10112,   -9347,   -8578,   -7805,   -7023,
    -6241,   -5453,   -4662,   -3868,   -3073,   -2274,   -1474,    -674,
      126,     929,    1729,    2527,    3326,    4123,    4916,    5707,
     6495,    7278,    8057,    8833,    9601,   10366,   11122,   11874,
    12618,   13353,   14082,   14802,   15512,   16216,   16906,   17589,
    18260,   18922,   19569,   20207,   20834,   21446,   22045,   22634,
    23206,   23765,   24311,   24842,   25357,   25858,   26343,   26812,
    27266,   27701,   28123,   28526,   28912,   29281,   29632,   29966,
    30281,   30579,   30859,   31118,   31361,   31583,   31788,   31973,
    32139,   32286,   32412,   32521,   32608,   32679,   32725,   32757,
    32766,   32757,   32725,   32679,   32608,   32521,   32412,   32286,
    32139,   31973,   31788,   31583,   31361,   31118,   30859,   30579,
    30281,   29966,   29632,   29281,   28912,   28526,   28123,   27701,
    27266,   26812,   26343,   25858,   25357,   24842,   24311,   23765,
    23206,   22634,   22045,   21446,   20834,   20207,   19569,   18922,
    18260,   17589,   16906,   16216,   15512,   14802,   14082,   13353,
    12618,   11874,   11122,   10366,    9601,    8833,    8057,    7278,
     6495,    5707,    4916,    4123,    3326,    2527,    1729,     929,
      126,    -674,   -1474,   -2274,   -3073,   -3868,   -4662,   -5453,
    -6241,   -7023,   -7805,   -8578,   -9347,  -10112,  -10868,  -11620,
   -12363,  -13100,  -13828,  -14548,  -15259,  -15960,  -16654,  -17334,
   -18006,  -18667,  -19316,  -19954,  -20579,  -21191,  -21793,  -22378,
   -22953,  -23512,  -24056,  -24588,  -25103,  -25604,  -26089,  -26558,
   -27011,  -27449,  -27868,  -28272,  -28658,  -29026,  -29379,  -29712,
   -30028,  -30324,  -30605,  -30864,  -31106,  -31331,  -31533,  -31719,
   -31885,  -32031,  -32160,  -32265,  -32356,  -32423,  -32473,  -32502,
   -32512
};
inline int32_t NextNoiseSample(uint32_t* state) {
  *state = *state * 1664525u + 1013904223u;
  return static_cast<int32_t>(static_cast<int16_t>(*state >> 16));
}
inline int32_t InterpolateSine(uint32_t phase) {
  const int32_t a = kBraidsSine[phase >> 24];
  const int32_t b = kBraidsSine[(phase >> 24) + 1];
  const int32_t fraction = static_cast<int32_t>((phase >> 8) & 0xffff);
  return a + ((b - a) * fraction >> 16);
}
inline int32_t TruncatedSine(uint32_t phase) {
  return kBraidsSine[(phase >> 22) & 0xff];
}
inline int32_t ToBraidsParameter(float normalized) {
  float value = normalized * 32767.0f;
  CONSTRAIN(value, 0.0f, 32767.0f);
  return static_cast<int32_t>(value);
}

}  // namespace

void QuestionMark::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}
void QuestionMark::Strike() {
  key_ = 0;
  tick_phase_ = 0;
  ticks_left_ = kQuestionMarkLeadInTicks;
  symbol_index_ = static_cast<uint32_t>(-1);
  seed_ = kQuestionMarkSeedAtStrike;
}

void QuestionMark::Reset() {
  phase_ = 0;
  Strike();
  rng_state_ = kQuestionMarkRngSeed;
  rng_state_aux_ = kQuestionMarkRngSeedAux;
  dc_state_ = 0.0f;
  dc_state_aux_ = 0.0f;
}

void QuestionMark::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    Strike();
  }

  const bool stereo = false;
  float note = parameters.note;
  if (note > kQuestionMarkHighestNote) {
    note = kQuestionMarkHighestNote;
  }
  const float increment_per_substep = NoteToFrequency(note) * 0.5f;
  const int32_t timbre = ToBraidsParameter(parameters.timbre);
  const int32_t color = ToBraidsParameter(parameters.harmonics);
  const uint32_t dit_duration = static_cast<uint32_t>(
      kQuestionMarkDitBase + ((32767 - timbre) >> 2));
  const int32_t noise_threshold =
      kQuestionMarkNoiseFloorBase + (color >> 3);
  const int32_t bed_gain = static_cast<int32_t>(
      BedGain(parameters.morph) * 32768.0f);
  const int32_t grit = static_cast<int32_t>(
      static_cast<float>(color) *
      1.0f);

  uint32_t phase = phase_;
  uint32_t rng_state = rng_state_;
  uint32_t rng_state_aux = rng_state_aux_;

  for (size_t i = 0; i < size; ++i) {
    float modulated_increment = increment_per_substep;
    const uint32_t increment = static_cast<uint32_t>(modulated_increment * 4294967296.0f);
    int32_t accumulator = 0;
    int32_t accumulator_aux = 0;
    for (int j = 0; j < 2; ++j) {
      phase += increment;
      const int32_t tone = key_
          ? (InterpolateSine(phase) * 3) >> 2
          : 0;
      if (++tick_phase_ > dit_duration) {
        --ticks_left_;
        if (ticks_left_ == 0) {
          ++symbol_index_;
          key_ = !key_;

          const size_t address = symbol_index_ >> 2;
          const size_t shift = (symbol_index_ & 0x3) << 1;
          ticks_left_ = static_cast<int16_t>(
              (2 << ((kMorseCode[address] >> shift) & 3)) - 1);
          if (ticks_left_ == 15) {
            ticks_left_ = kQuestionMarkTerminatorTicks;
            key_ = 0;
            symbol_index_ = static_cast<uint32_t>(-1);
          }
          phase = 1L << 30;
        }
        tick_phase_ = 0;
      }
      seed_ += NextNoiseSample(&rng_state) >> 2;
      int32_t intensity = seed_ >> 8;
      if (intensity < 0) {
        intensity = -intensity;
      }
      if (intensity < noise_threshold) {
        intensity = noise_threshold;
      }
      if (intensity > kQuestionMarkMaxIntensity) {
        intensity = kQuestionMarkMaxIntensity;
      }
      int32_t noise = (NextNoiseSample(&rng_state) * intensity) >> 15;
      noise = noise * TruncatedSine(phase) >> 15;
      if (bed_gain != 32768) {
        noise = noise * bed_gain >> 15;
      }
      int32_t sample = tone + noise;
      CLIP(sample);
      const int32_t distorted = sample * sample >> 14;
      sample += static_cast<int32_t>(
          (static_cast<int64_t>(distorted) * grit) >> 15);
      CLIP(sample);
      accumulator += sample;

      if (stereo) {
        int32_t noise_aux =
            (NextNoiseSample(&rng_state_aux) * intensity) >> 15;
        noise_aux = noise_aux * TruncatedSine(phase) >> 15;
        if (bed_gain != 32768) {
          noise_aux = noise_aux * bed_gain >> 15;
        }
        int32_t sample_aux = tone + noise_aux;
        CLIP(sample_aux);
        const int32_t distorted_aux = sample_aux * sample_aux >> 14;
        sample_aux += static_cast<int32_t>(
            (static_cast<int64_t>(distorted_aux) * grit) >> 15);
        CLIP(sample_aux);
        accumulator_aux += sample_aux;
      } else {
        accumulator_aux += tone;
      }
    }
    const float raw = static_cast<float>(accumulator) * (0.5f / 32768.0f);
    const float raw_aux =
        static_cast<float>(accumulator_aux) * (0.5f / 32768.0f);
    ONE_POLE(dc_state_, raw, kQuestionMarkDcCoefficient);
    ONE_POLE(dc_state_aux_, raw_aux, kQuestionMarkDcCoefficient);
    *out++ = raw - dc_state_;
    *aux++ = raw_aux - dc_state_aux_;
  }

  phase_ = phase;
  rng_state_ = rng_state;
  rng_state_aux_ = rng_state_aux;
}

}  // namespace plaits
