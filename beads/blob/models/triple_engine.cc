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
// Triple: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/triple_engine.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"

#include "beads/blob/models/triple_engine_data.h"
#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {
inline float LadderDetune(float knob) {
  const int p = static_cast<int>(knob * 32767.0f);
  int i1 = p >> 9;
  int i2 = ((p >> 8) + 1) >> 1;
  CONSTRAIN(i1, 0, kTripleNumIntervals - 1);
  CONSTRAIN(i2, 0, kTripleNumIntervals - 1);
  const int xfade = (p << 8) & 0xffff;
  const int detune_1 = static_cast<int>(kTripleIntervals[i1] * 128.0f);
  const int detune_2 = static_cast<int>(kTripleIntervals[i2] * 128.0f);
  const int detune = detune_1 + (((detune_2 - detune_1) * xfade) >> 16);
  return static_cast<float>(detune) * (1.0f / 128.0f);
}

}  // namespace

void Triple::Init(BufferAllocator* allocator) {
  (void) allocator;
  for (int i = 0; i < kTripleNumVoices; ++i) {
    voice_[i].Init();
  }
  Reset();
}

void Triple::Reset() {
  for (int i = 0; i < kTripleNumVoices; ++i) {
    voice_[i].Init();
    sine_phase_[i] = 0.0f;
  }
}

void Triple::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    Reset();
  }

  const float root = NoteToFrequency(parameters.note);
  float frequency[kTripleNumVoices];
  frequency[0] = root;
  frequency[1] = NoteToFrequency(
      parameters.note + LadderDetune(parameters.harmonics));
  frequency[2] = NoteToFrequency(
      parameters.note + LadderDetune(parameters.timbre));
  const float shaped_morph = min(
      parameters.morph / kTripleSineRegion, 1.0f);
  const float waveshape = 1.0f - shaped_morph;
  const float sine_amount = parameters.morph <= kTripleSineRegion
      ? 0.0f
      : (parameters.morph - kTripleSineRegion) / (1.0f - kTripleSineRegion);
  const float shape = 0.5f;
  const float pw = 1.0f - shape;

  float voice_buffer[kMaxBlockSize];

  for (size_t i = 0; i < size; ++i) {
    out[i] = 0.0f;
    aux[i] = 0.0f;
  }
  const float square_amount = max(waveshape - 0.5f, 0.0f) * 2.0f;

  for (int v = 0; v < kTripleNumVoices; ++v) {
    float voice_pw = pw;
    if (frequency[v] >= kMaxFrequency) {
      voice_pw = 0.5f;
    } else {
      CONSTRAIN(voice_pw, frequency[v] * 2.0f, 1.0f - 2.0f * frequency[v]);
    }
    const float dc = square_amount * (1.0f - 2.0f * voice_pw);

      voice_[v].Render(frequency[v], pw, waveshape, voice_buffer, size);
    for (size_t i = 0; i < size; ++i) {
      float sample = voice_buffer[i] - dc;
      if (sine_amount > 0.0f) {
        float sine_frequency = frequency[v];
        sine_phase_[v] += sine_frequency;
        if (sine_phase_[v] >= 1.0f) {
          sine_phase_[v] -= 1.0f;
        } else if (sine_phase_[v] < 0.0f) {
          sine_phase_[v] += 1.0f;
        }
        const float sine = Sine(sine_phase_[v]);
        sample += (sine - sample) * sine_amount;
      }
      out[i] += sample * kTripleVoiceGain;
      if (v == 0) {
        aux[i] = sample;
      }
    }
  }
}

}  // namespace plaits
