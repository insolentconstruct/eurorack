// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct
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
// Abyss impulse response, gravity, freeze and rate checks.

#include <cmath>
#include <cstring>

#include "beads/dsp/fx/abyss.h"
#include "beads/test/test_checker.h"

using namespace beads;

float bank_0[512 * 1024 / sizeof(float)];
float bank_1[256 * 1024 / sizeof(float)];

struct Response {
  double energy[10];
  int first;
};

Response Render(int rate, const Abyss::Controls& c, bool hold) {
  Abyss abyss;
  memset(bank_0, 0xa5, sizeof(bank_0));
  memset(bank_1, 0x5a, sizeof(bank_1));
  abyss.Init(reinterpret_cast<uint8_t*>(bank_0),
      reinterpret_cast<uint8_t*>(bank_1), float(rate));

  Response response = { { 0.0 }, -1 };
  const int block_size = rate / 4000;
  for (int n = 0; n < 10 * rate; n += block_size) {
    FloatFrame in[12];
    FloatFrame out[12];
    bool freeze[12];
    for (int i = 0; i < block_size; ++i) {
      in[i].l = n + i == rate / 4 ? 0.8f : 0.0f;
      in[i].r = 0.0f;
      freeze[i] = hold && n + i >= rate + 1;
    }
    abyss.Process(c, freeze, in, out, block_size);
    for (int i = 0; i < block_size; ++i) {
      const float l = out[i].l;
      const float r = out[i].r;
      CHECK(std::isfinite(l) && std::isfinite(r));
      CHECK(fabsf(l) < 2.0f && fabsf(r) < 2.0f);
      const double energy = double(l) * l + double(r) * r;
      const int t = n + i;
      if (response.first < 0 && t > rate / 4 && energy > 1e-9) {
        response.first = t;
      }
      response.energy[t / rate] += energy;
    }
  }
  return response;
}

void TestAbyss() {
  test_checker.Start("TestAbyss");
  const int rates[] = { 48000, 32000, 24000 };
  for (size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); ++i) {
    const int rate = rates[i];
    Abyss::Controls c = { 0.8f, 0.85f, 0.7f, 0.65f, 0.5f, 0.0f, 0.5f, 1.0f };
    const Response normal = Render(rate, c, false);
    CHECK(fabsf(float(normal.first) / float(rate) - 0.378f) < 0.006f);
    CHECK(normal.energy[4] > 1e-5);
    CHECK(normal.energy[9] < normal.energy[4]);
    const Response frozen = Render(rate, c, true);
    CHECK(frozen.energy[9] > 0.65 * frozen.energy[3]);
    CHECK(frozen.energy[9] < 1.5 * frozen.energy[3]);
    CHECK(frozen.energy[9] > 10.0 * normal.energy[9]);
    c.gravity = 0.0f;
    const Response bloom = Render(rate, c, false);
    CHECK(bloom.energy[0] < 0.07 * normal.energy[0]);
    CHECK(fabs(bloom.energy[3] - normal.energy[3]) < 1e-6);
    c.gravity = 1.0f;
    const Response inverse = Render(rate, c, false);
    CHECK(fabs(inverse.energy[0] - bloom.energy[0]) < 1e-6);
    CHECK(inverse.energy[1] > inverse.energy[0]);
    CHECK(inverse.energy[4] < 1e-10);
    c.gravity = 0.5f;
    c.pitch = 0.6f;
    const Response shimmer = Render(rate, c, false);
    CHECK(shimmer.energy[2] != normal.energy[2]);
    c.pitch = -1.0f;
    c.room = 1.0f;
    c.drift = 1.0f;
    c.decay = 1.0f;
    const Response large = Render(rate, c, false);
    CHECK(large.first > normal.first);
    c.mix = 0.0f;
    const Response dry = Render(rate, c, false);
    CHECK(dry.first == -1 && dry.energy[0] > 0.6);
  }
}

void TestPitch() {
  test_checker.Start("TestPitch");
  for (int sign = -1; sign <= 1; sign += 2) {
    CHECK(Abyss::PitchOnNotch(sign * 0.05f));
    CHECK(!Abyss::PitchOnNotch(sign * 0.06f));
    CHECK(!Abyss::PitchOnNotch(float(sign)));
    const Abyss::Pitch centre = Abyss::MapPitch(sign * 0.05f);
    CHECK(centre.detune_mix == 0.0f);
    CHECK(centre.fifth_mix == 0.0f && centre.octave_mix == 0.0f);
    const Abyss::Pitch detune = Abyss::MapPitch(sign * 0.33f);
    CHECK(detune.detune == 0.09f && detune.detune_mix == 0.2f);
    CHECK(detune.fifth_mix == 0.0f && detune.octave_mix == 0.0f);
    const Abyss::Pitch fifth = Abyss::MapPitch(sign * 0.6f);
    CHECK(fifth.fifth == sign * 7.0f && fifth.fifth_mix == 0.2f);
    CHECK(fifth.detune_mix == 0.0f && fifth.octave_mix == 0.0f);
    const Abyss::Pitch end = Abyss::MapPitch(sign);
    CHECK(end.octave == sign * 12.0f && end.octave_mix == 0.2f);
    CHECK(end.detune_mix == 0.0f && end.fifth_mix == 0.0f);
  }
  for (int i = -100; i <= 100; ++i) {
    const Abyss::Pitch p = Abyss::MapPitch(float(i) / 100.0f);
    CHECK(p.detune_mix + p.fifth_mix + p.octave_mix <= 0.200001f);
  }
}

int main() {
  TestAbyss();
  TestPitch();
  return test_checker.failures() ? 1 : 0;
}
