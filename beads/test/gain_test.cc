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
// Input gain DSP tests.

#include <cmath>

#include "beads/gain_control.h"
#include "beads/test/test_checker.h"

using namespace beads;

// Static firmware storage zero-initializes manual_gain_ before Init().
static GainControl control;

FloatFrame Render(float level, uint8_t gain, int blocks, bool active = true) {
  Codec::Frame in[24];
  FloatFrame out[24];
  for (int i = 0; i < 24; ++i) {
    in[i].l = int32_t(level * 2147483648.0f);
    in[i].r = -in[i].l;
  }
  for (int i = 0; i < blocks; ++i) {
    control.Process(active, gain, false, false, in, out, 24);
  }
  return out[23];
}

void TestGainLearningWindow() {
  test_checker.Start("TestGainLearningWindow");
  control.Init(48000.0f, GainControl::SOFT_CLIPPING_MODE_OFF, true);
  Render(0.1f, 0, 1);
  CHECK(control.adjusting());
  Render(0.1f, 0, 1999);
  CHECK(control.adjusting());
  // The first second holds the detector at its initial value.
  CHECK(fabsf(Render(0.1f, 0, 1).l - 0.18f) < 0.001f);
  Render(0.1f, 0, 9998);
  CHECK(control.adjusting());
  Render(0.1f, 0, 1);
  CHECK(!control.adjusting());
  control.Restart();
  CHECK(control.adjusting());
  Render(0.1f, 1, 1);
  CHECK(!control.adjusting());
}

void TestGainHeadroomAndRise() {
  test_checker.Start("TestGainHeadroomAndRise");
  control.Init(48000.0f, GainControl::SOFT_CLIPPING_MODE_OFF, true);
  const float learned = Render(0.05f, 0, 14000).l;
  CHECK(fabsf(learned - 0.9f) < 0.002f);
  const float transient = Render(0.5f, 0, 1).l;
  const float settled = Render(0.5f, 0, 6000).l;
  CHECK(transient > 1.0f);
  CHECK(settled < transient * 0.2f);
  CHECK(fabsf(settled - 0.9f) < 0.002f);
}

void TestGainDigitalClipping() {
  test_checker.Start("TestGainDigitalClipping");
  control.Init(48000.0f, GainControl::SOFT_CLIPPING_MODE_OFF, true);
  const FloatFrame clean = Render(0.01f, 255, 2000);
  CHECK(fabsf(clean.l - 0.4f) < 0.0001f);
  CHECK(fabsf(clean.r + 0.4f) < 0.0001f);
  const FloatFrame clipped = Render(0.1f, 255, 1);
  CHECK(clipped.l == 1.0f);
  CHECK(clipped.r == -1.0f);
}

void TestGainTapeClipping() {
  test_checker.Start("TestGainTapeClipping");
  const GainControl::SoftClippingMode modes[] = {
    GainControl::SOFT_CLIPPING_MODE_ON,
    GainControl::SOFT_CLIPPING_MODE_DYNAMIC
  };
  for (int i = 0; i < 2; ++i) {
    control.Init(48000.0f, modes[i], true);
    const FloatFrame knee = Render(0.025f, 255, 2000);
    CHECK(knee.l > 0.6f && knee.l < 0.9f);
    CHECK(knee.r < -0.6f && knee.r > -0.9f);
    const FloatFrame overload = Render(0.1f, 255, 2000);
    CHECK(overload.l > knee.l && overload.l < 1.0f);
    CHECK(overload.r < knee.r && overload.r >= -1.0f);
  }
}

void TestGainNoiseAmplification() {
  test_checker.Start("TestGainNoiseAmplification");
  control.Init(48000.0f, GainControl::SOFT_CLIPPING_MODE_OFF, true);
  Codec::Frame in[24];
  FloatFrame out[24];
  uint32_t seed = 1;
  double early = 0.0;
  double late = 0.0;
  for (int block = 0; block < 16000; ++block) {
    for (int i = 0; i < 24; ++i) {
      seed = seed * 1664525u + 1013904223u;
      in[i].l = (int32_t(seed >> 16) - 32768) * 32;
      in[i].r = in[i].l;
    }
    control.Process(true, 0, false, false, in, out, 24);
    for (int i = 0; i < 24; ++i) {
      if (block < 100) {
        early += out[i].l * out[i].l;
      }
      if (block >= 15900) {
        late += out[i].l * out[i].l;
      }
    }
  }
  CHECK(late > early * 100.0);
  CHECK(fabsf(out[23].l / (float(in[23].l) / 2147483648.0f)
      - 40.0f) < 0.001f);
}

int main() {
  TestGainLearningWindow();
  TestGainHeadroomAndRise();
  TestGainDigitalClipping();
  TestGainTapeClipping();
  TestGainNoiseAmplification();
  return test_checker.failures() ? 1 : 0;
}
