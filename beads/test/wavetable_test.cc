// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
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
// -----------------------------------------------------------------------------
//
// Final wavetable interpolation row tests.

#include "beads/test/test_checker.h"
#include <cmath>
#include <cstring>
#include "stmlib/dsp/dsp.h"
#include "beads/resources.h"
#include "beads/dsp/frame.h"
#include "beads/dsp/granular_player.h"
#include "stmlib/dsp/units.h"
#define private public
#include "beads/dsp/wavetable_grain.h"
#include "beads/dsp/wavetable_voice.h"
#undef private

using namespace beads;

void TestFinalBankRendering() {
  test_checker.Start("TestFinalBankRendering");
  const int stride = kTableSize + 4;
  const int rows = WAV_INTEGRATED_WAVES_SIZE / stride;
  const int bank = (rows - 1) / kNumWavesPerTable - 1;
  CHECK(rows == 193 && bank == 11);
  const float positions[] = { 0.999f, 1.0f };
  for (int i = 0; i < 2; ++i) {
    WavetableGrain grain;
    grain.Init(48000);
    grain.Start(bank, 16, positions[i], 0.00371f, 4096, 0.5f,
        0.5f, 0.5f, 0.37f);
    for (int n = 0; n < 4096; ++n) {
      float phase = (float(n) + 0.37f) / 4096.0f;
      CHECK(std::isfinite(grain.ReadSample(phase)));
      FloatFrame out = {};
      grain.Render(&out);
      CHECK(std::isfinite(out.l) && std::isfinite(out.r));
    }
    WavetableVoice voice;
    voice.Init(48000);
    Parameters p = {};
    p.time.base = positions[i];
    bool freeze = false;
    for (int n = 0; n < 8192; ++n) {
      FloatFrame out = {};
      voice.Render(bank, 16, &freeze, p, &out, 1);
      CHECK(std::isfinite(out.l) && std::isfinite(out.r));
      CHECK(fabsf(out.l) <= 1.0f && out.l == out.r);
    }
    CHECK(voice.bank_ == bank && voice.wave_lp_ > 0.998f);
    // Force the exact endpoint; the smoother otherwise approaches it from below.
    voice.wave_lp_ = 1.0f;
    p.time.base = 1.0f;
    FloatFrame out = {};
    voice.Render(bank, 16, &freeze, p, &out, 1);
    CHECK(std::isfinite(out.l) && std::isfinite(out.r));
  }
}

void TestGuardRow() {
  test_checker.Start("TestGuardRow");
  const int stride = kTableSize + 4;
  const int rows = WAV_INTEGRATED_WAVES_SIZE / stride;
  CHECK(!memcmp(wav_integrated_waves + (rows - 2) * stride,
      wav_integrated_waves + (rows - 1) * stride, stride * sizeof(int16_t)));
}

int main() {
  TestFinalBankRendering();
  TestGuardRow();
  return test_checker.failures() ? 1 : 0;
}
