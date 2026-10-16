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
// Clouds character and stock regression tests.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "stmlib/utils/random.h"

#include "beads/dsp/processor.h"
#include "beads/dsp/synth.h"
#ifndef REFERENCE_CAPTURE
#include "beads/dsp/fx/diffuser.h"
#endif
#include "beads/test/test_checker.h"

using namespace beads;

// Captured from da792fd, GCC host build with contraction disabled.
const uint32_t kStockHashes[4][3] = {
  { 0x6eac02c8, 0x72c5604b, 0x27adc2e7 },
  { 0x79a67055, 0x2d9eb3b9, 0x0e6f80d8 },
  { 0x4d4af04d, 0xe5c6ff67, 0x971a869b },
  { 0x88735508, 0x780972b3, 0xf0642700 }
};


const uint32_t kCloudsHashes[4][3] = {
  { 0x9ef8ffbf, 0x585619e3, 0xd04954b1 },
  { 0x455c8d06, 0x6246c9f2, 0x6c42e900 },
  { 0x1ba970f7, 0x85b58047, 0x750dd32f },
  { 0xa8f5e418, 0xa612bb0c, 0x3bcf8e5c }
};


static Processor processor;
static Synth synth;
static uint8_t banks[3][262144];
static uint16_t reverb[32768];

uint32_t Render(int quality, int mode, bool clouds) {
  memset(static_cast<void*>(&processor), 0, sizeof(processor));
  memset(static_cast<void*>(&synth), 0, sizeof(synth));
  memset(banks, 0, sizeof(banks));
  memset(reverb, 0, sizeof(reverb));
  uint8_t* buffers[] = { banks[0], banks[1], banks[2] };
  processor.Init(buffers, reverb);
  processor.Configure(Processor::Media(quality), false);
  synth.Init(reverb);
#ifndef REFERENCE_CAPTURE
  if (clouds) {
    processor.SetCloudsStyle(true);
    synth.SetCloudsStyle(true);
  }
#endif
  stmlib::Random::Seed(0x12345678);
  Parameters p;
  memset(&p, 0, sizeof(p));
  p.time.base = 0.5f;
  p.size.base = mode == 1 ? 1.0f : 0.5f;
  p.shape.base = 0.5f;
  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.8f;
  ModulatedParameter density;
  density.base = 0.95f;
  density.cv = 0.0f;
  Processor::MixParameters mix;
  mix.feedback = 0.8f;
  mix.dry_wet = 0.8f;
  mix.reverb_amount = 0.7f;
  mix.output_gate = false;
  const size_t size = mode == 2 ? 12 :
      size_t(processor.sample_rate()) / 4000;
  uint32_t hash = 2166136261U;
  double energy = 0.0;
  for (size_t block = 0; block < 8000; ++block) {
    FloatFrame in[12];
    FloatFrame out[12];
    bool freeze[12] = { };
    for (size_t i = 0; i < size; ++i) {
      in[i].l = float(int((block * size + i) * 73 % 2048) - 1024) / 8192.0f;
      in[i].r = float(int((block * size + i) * 137 % 2048) - 1024) / 8192.0f;
    }
    // Exercise gate output too, without affecting the preceding history.
    mix.output_gate = block >= 7500;
    if (mode == 2) {
      synth.Render(SEED_BUTTON_STATE_ON, NULL, freeze,
          Synth::Resolution(quality), density, p, mix, out, size);
    } else {
      processor.Process(SEED_BUTTON_STATE_ON, NULL, freeze,
          density, p, mix, in, out, size);
    }
    for (size_t i = 0; i < size; ++i) {
      CHECK(std::isfinite(out[i].l) && std::isfinite(out[i].r));
      CHECK(fabsf(out[i].l) < 4.0f && fabsf(out[i].r) < 4.0f);
      if (block >= 7000) {
        energy += out[i].l * out[i].l + out[i].r * out[i].r;
      }
    }
    if (block >= 7000) {
      const uint8_t* bytes = reinterpret_cast<const uint8_t*>(out);
      for (size_t i = 0; i < size * sizeof(FloatFrame); ++i) {
        hash = (hash ^ bytes[i]) * 16777619U;
      }
    }
  }
  CHECK(energy > 0.01);
  return hash;
}


void TestStockHashes() {
  test_checker.Start("TestStockHashes");
  for (int q = 0; q < 4; ++q) {
    for (int mode = 0; mode < 3; ++mode) {
      const uint32_t hash = Render(q, mode, false);
      printf("Stock q%d mode%d %08x\n", q, mode, hash);
#ifndef REFERENCE_CAPTURE
      CHECK(hash == kStockHashes[q][mode]);
#endif
    }
  }
}


void TestCloudsOutput() {
  test_checker.Start("TestCloudsOutput");
#ifndef REFERENCE_CAPTURE
  for (int q = 0; q < 4; ++q) {
    for (int mode = 0; mode < 3; ++mode) {
      const uint32_t hash = Render(q, mode, true);
      printf("Clouds q%d mode%d %08x\n", q, mode, hash);
      CHECK(hash != kStockHashes[q][mode]);
      CHECK(hash == kCloudsHashes[q][mode]);
    }
  }
#endif
}

void TestDiffuserImpulse() {
  test_checker.Start("TestDiffuserImpulse");
#ifndef REFERENCE_CAPTURE
  uint16_t buffer[2048];
  Diffuser diffuser;
  diffuser.Init(buffer);
  diffuser.set_amount(1.0f);
  double left = 0.0;
  double right = 0.0;
  for (int i = 0; i < 32000; ++i) {
    FloatFrame frame;
    frame.l = i == 0 ? 0.25f : 0.0f;
    frame.r = i == 0 ? 0.125f : 0.0f;
    diffuser.Process(&frame, 1);
    CHECK(std::isfinite(frame.l) && std::isfinite(frame.r));
    left += frame.l * frame.l;
    right += frame.r * frame.r;
  }
  CHECK(fabs(left / (0.25 * 0.25) - 1.0) < 0.03);
  CHECK(fabs(right / (0.125 * 0.125) - 1.0) < 0.05);
#endif
}

int main() {
  setbuf(stdout, NULL);
  TestStockHashes();
  TestCloudsOutput();
  TestDiffuserImpulse();
  return test_checker.failures() ? 1 : 0;
}
