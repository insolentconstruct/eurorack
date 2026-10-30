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
#endif  // REFERENCE_CAPTURE
#include "beads/test/test_checker.h"
#include "beads/test/test_runner.h"

using namespace beads;

// Captures use explicit grain-pan FMA and rate-aware Clouds modulation.
// GCC 12 (amd64), -ffp-contract=off; make -f beads/test/clouds.mk capture.
#if defined(__aarch64__)
const uint32_t kStockSynthHashes[4] = {
  0xe67ef064, 0xdde6451f, 0xde7f8524, 0x1777a760
};
const uint32_t kCloudsSynthHashes[4] = {
  0x90c58d5b, 0xb09a99f3, 0x937f6b89, 0xb70a52f8
};
#else
const uint32_t kStockSynthHashes[4] = {
  0x27adc2e7, 0x0e6f80d8, 0x971a869b, 0xf0642700
};
const uint32_t kCloudsSynthHashes[4] = {
  0x221c8e17, 0x431d2106, 0x3e9aaca8, 0x3335d7a1
};
#endif  // defined(__aarch64__)

const uint32_t kStockHashes[4][2] = {
  { 0x6eac02c8, 0x72c5604b },
  { 0x79a67055, 0x2d9eb3b9 },
  { 0x4d4af04d, 0xe5c6ff67 },
  { 0xb7c98055, 0x780972b3 }
};
const uint32_t kCloudsHashes[4][2] = {
  { 0xebf12137, 0x3df5b30e },
  { 0xd01041dd, 0x5791b19d },
  { 0x1ba970f7, 0x85b58047 },
  { 0x9cad2330, 0x8955f0ae }
};

// Beta 1 (2026.10.18) with grains playing (TIME at the newest audio): a saved
// diffusion of 0 must keep both reverbs identical.
const uint32_t kGrainStockHashes[4] = {
  0xa0d55183, 0xddb55757, 0x69ce6cc4, 0x6c919469
};
const uint32_t kGrainCloudsHashes[4] = {
  0x29eb3196, 0x9009e699, 0x1149a241, 0x5244e4e0
};

static Processor processor;
static Synth synth;
static uint8_t banks[3][262144];
static uint16_t reverb[32768];

// A negative diffusion follows the reverb character, as a saved zero does.
uint32_t Render(int quality, int mode, bool clouds, bool reconfigure,
                float diffusion, float time) {
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
    synth.SetCloudsStyle(true, true);
    if (reconfigure) {
      processor.Configure(Processor::Media(quality), false);
    }
  }
  processor.set_diffusion(diffusion >= 0.0f
      ? diffusion : (clouds ? kCloudsDiffusion : 0.0f));
#endif  // REFERENCE_CAPTURE
  stmlib::Random::Seed(0x12345678);
  Parameters p;
  memset(&p, 0, sizeof(p));
  p.time.base = time;
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
      const uint32_t hash = Render(q, mode, false, false, -1.0f, 0.5f);
      printf("Stock q%d mode%d %08x\n", q, mode, hash);
#if !defined(REFERENCE_CAPTURE) && !defined(GOLDEN_CAPTURE)
      CHECK(hash == (mode == 2 ? kStockSynthHashes[q] : kStockHashes[q][mode]));
#endif  // !defined(REFERENCE_CAPTURE) && !defined(GOLDEN_CAPTURE)
    }
  }
}


void TestCloudsOutput() {
  test_checker.Start("TestCloudsOutput");
#ifndef REFERENCE_CAPTURE
  for (int q = 0; q < 4; ++q) {
    for (int mode = 0; mode < 3; ++mode) {
      const uint32_t hash = Render(q, mode, true, false, -1.0f, 0.5f);
      printf("Clouds q%d mode%d %08x\n", q, mode, hash);
      CHECK(hash != (mode == 2 ? kStockSynthHashes[q] : kStockHashes[q][mode]));
#ifndef GOLDEN_CAPTURE
      CHECK(hash == (mode == 2
          ? kCloudsSynthHashes[q] : kCloudsHashes[q][mode]));
#endif  // GOLDEN_CAPTURE
      if (mode == 0) {
        CHECK(hash == Render(q, mode, true, true, -1.0f, 0.5f));
      }
    }
  }
#endif  // REFERENCE_CAPTURE
}

// Compare Clouds initialization with an explicit stock-style rate reset.
// This checks the wet output, including the oscillator and delay engine.
void TestReverbRates() {
  test_checker.Start("TestReverbRates");
  for (int rate = 32000; rate <= 48000; rate += 16000) {
    static uint16_t memory[2][32768];
    Reverb actual;
    Reverb reference;
    memset(static_cast<void*>(&actual), 0, sizeof(actual));
    memset(static_cast<void*>(&reference), 0, sizeof(reference));
    actual.Init(memory[0], rate);
    reference.Init(memory[1], rate);
    actual.Reset(rate, true);
    reference.Reset(rate);
    actual.set_mix(1.0f);
    reference.set_mix(1.0f);
    actual.set_time(0.7f);
    reference.set_time(0.7f);
    double energy = 0.0;
    for (int n = 0; n < rate * 3; n += 12) {
      FloatFrame a[12] = { };
      FloatFrame b[12] = { };
      for (int i = 0; i < 12; ++i) {
        a[i].l = b[i].l = float((n + i) % 127 - 63) / 256.0f;
      }
      actual.ProcessClouds(a, 12);
      reference.ProcessClouds(b, 12);
      CHECK(memcmp(a, b, sizeof(a)) == 0);
      for (int i = 0; i < 12; ++i) {
        energy += a[i].l * a[i].l;
      }
    }
    CHECK(energy > 0.01);
  }
}

// The amount set with [A]+REVERB diffuses the grains with either reverb.
void TestDiffusionAmount() {
  test_checker.Start("TestDiffusionAmount");
#ifndef REFERENCE_CAPTURE
  for (int q = 0; q < 4; ++q) {
    // TIME at the newest audio: the 2 s render has not filled the buffer.
    const float time = 0.02f;
    const uint32_t stock = Render(q, 0, false, false, 0.0f, time);
    const uint32_t diffused = Render(q, 0, false, false, 0.5f, time);
    const uint32_t clouds = Render(q, 0, true, false, -1.0f, time);
    const uint32_t clouds_dry = Render(q, 0, true, false, 0.0f, time);
    printf("Diffusion q%d stock %08x +0.5 %08x clouds %08x +0 %08x\n",
        q, stock, diffused, clouds, clouds_dry);
    CHECK(stock == kGrainStockHashes[q]);
    CHECK(clouds == kGrainCloudsHashes[q]);
    CHECK(diffused != stock);
    CHECK(clouds_dry != clouds);
    // Delay mode bypasses the diffuser, as before.
    CHECK(Render(q, 1, false, false, 0.5f, 0.5f) == kStockHashes[q][1]);
  }
#endif  // REFERENCE_CAPTURE
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
#endif  // REFERENCE_CAPTURE
}

int main(int argc, char** argv) {
  setbuf(stdout, NULL);
  const HostTest tests[] = {
    { "TestStockHashes", TestStockHashes },
    { "TestCloudsOutput", TestCloudsOutput },
    { "TestReverbRates", TestReverbRates },
    { "TestDiffusionAmount", TestDiffusionAmount },
    { "TestDiffuserImpulse", TestDiffuserImpulse },
  };
  const int result = RunHostTests(argc, argv, tests,
      sizeof(tests) / sizeof(tests[0]));
  return result ? result : (test_checker.failures() ? 1 : 0);
}
