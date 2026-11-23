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
// SawComb: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/saw_comb_engine.h"

#include <algorithm>
#include <cmath>

#include "stmlib/dsp/dsp.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

inline float WarpResonance(float x) {
  CONSTRAIN(x, -1.0f, kSawCombWarpTop);
  return kSawCombWarpScale * (tanhf(2.0f * x) - kSawCombWarpMean);
}

inline float ReadLine(const int16_t* line, uint32_t write_pointer,
                      float delay) {
  const uint32_t integral = static_cast<uint32_t>(delay);
  const float fractional = delay - static_cast<float>(integral);
  const uint32_t offset = write_pointer + 2 * kSawCombDelaySize - integral;
  const float a = static_cast<float>(
      line[offset & (kSawCombDelaySize - 1)]);
  const float b = static_cast<float>(
      line[(offset - 1) & (kSawCombDelaySize - 1)]);
  return (a + (b - a) * fractional) * (1.0f / 32768.0f);
}

}  // namespace

void SawComb::Init(BufferAllocator* allocator) {
  line_ = allocator->Allocate<int16_t>(kSawCombDelaySize);
  exciter_.Init();
  Reset();
}

void SawComb::Reset() {
  if (line_) {
    fill(&line_[0], &line_[kSawCombDelaySize], 0);
  }
  exciter_.Init();
  write_pointer_ = 0;
  filtered_pitch_ = 0;
  loop_lp_ = 0.0f;
}

void SawComb::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (!line_) {
    for (size_t i = 0; i < size; ++i) {
      out[i] = 0.0f;
      aux[i] = 0.0f;
    }
    return;
  }

  const float frequency = NoteToFrequency(parameters.note);

  int32_t note_pitch = static_cast<int32_t>(parameters.note * 128.0f);
  CONSTRAIN(note_pitch, kSawCombNoteFloor, kSawCombNoteCeiling);
  const int32_t target_pitch = note_pitch +
      ((static_cast<int32_t>(parameters.timbre * 32767.0f) - 16384) >> 1);
  size_t updates = size / kBlockSize;
  if (!updates) {
    updates = 1;
  }
  while (updates--) {
    filtered_pitch_ = (15 * filtered_pitch_ + target_pitch) >> 4;
  }

  int32_t comb_pitch = filtered_pitch_;
  if (comb_pitch >= kSawCombPitchCeiling) {
    comb_pitch = kSawCombPitchCeiling;
  }

  float delay = 1.0f / max(1e-7f,
      NoteToFrequency(static_cast<float>(comb_pitch) * (1.0f / 128.0f)));
  CONSTRAIN(delay, 2.0f, static_cast<float>(kSawCombDelaySize));

  float delay_aux = delay * 1.5f;
  if (delay_aux > static_cast<float>(kSawCombDelaySize)) {
    delay_aux = delay / 1.5f;
  }
  CONSTRAIN(delay_aux, 2.0f, static_cast<float>(kSawCombDelaySize));

  float feedback = WarpResonance(2.0f * parameters.harmonics - 1.0f);

  const float tilt = 0.0f;

  feedback *= 1.0f / (1.0f - kSawCombShelfHf * min(tilt, 0.0f));

  const float waveshape = 0.5f + 0.5f * parameters.morph;
  const float pw = 0.5f - 0.25f * parameters.morph;

  float exciter[kMaxBlockSize];
  exciter_.Render(frequency, pw, waveshape, exciter, size);

  for (size_t i = 0; i < size; ++i) {
    const float in = exciter[i];
    float sample_delay = delay;
    float sample_delay_aux = delay_aux;
    const float delayed = ReadLine(line_, write_pointer_, sample_delay);
    const float delayed_aux =
        ReadLine(line_, write_pointer_, sample_delay_aux);

    loop_lp_ += (delayed - loop_lp_) * kSawCombShelfPole;
    const float shaped = delayed + (loop_lp_ - delayed) * tilt;
    float written = (shaped * feedback + 0.5f * in) * 32768.0f;
    CONSTRAIN(written, -32768.0f, 32767.0f);
    line_[write_pointer_ & (kSawCombDelaySize - 1)] = static_cast<int16_t>(
        written + (written >= 0.0f ? 0.5f : -0.5f));
    ++write_pointer_;

    float main_out = 0.5f * in + delayed;
    CONSTRAIN(main_out, -1.0f, 1.0f);
    out[i] = main_out;
    float aux_out = 0.5f * in + delayed_aux;
    CONSTRAIN(aux_out, -1.0f, 1.0f);
    aux[i] = aux_out;
  }
}

}  // namespace plaits
