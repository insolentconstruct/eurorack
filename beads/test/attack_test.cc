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
// Attacks (#58, #47): detection on the input, marks in the recording, and
// grain starts moved onto them.

#include <cmath>
#include <cstdio>
#include <cstring>

#define private public
#include "beads/dsp/processor.h"
#undef private
#include "beads/test/test_checker.h"

using namespace beads;

struct CodecFrame {
  int32_t l;
  int32_t r;
};

const size_t kBlockSize = 12;
const float kRate = 48000.0f;
const float kScale = 1.0f / 2147483648.0f;

// One block at a level (fraction of full scale).
bool Feed(AttackInput* input, float level) {
  CodecFrame in[kBlockSize];
  for (size_t i = 0; i < kBlockSize; ++i) {
    const int32_t v = int32_t(level * 2147483647.0f) * (i & 1 ? -1 : 1);
    in[i].l = v;
    in[i].r = v;
  }
  return input->Process(in, kBlockSize, kScale, 0.0f, kRate, false, false);
}

void TestAttackInput() {
  test_checker.Start("TestAttackInput");
  AttackInput input;
  input.Init();
  CHECK(!Feed(&input, 0.5f));  // Off detects nothing.
  input.Configure(ATTACK_MODE_SNAP, kAttackThreshold);
  // Quiet, then a hit: one onset, then none during the refractory time.
  int onsets = 0;
  int first = -1;
  for (int b = 0; b < 400; ++b) {
    const float level = b < 200 ? 0.01f : 0.5f;
    if (Feed(&input, level)) {
      ++onsets;
      if (first < 0) {
        first = b;
      }
    }
  }
  CHECK(onsets == 1);
  CHECK(first == 200);
  // Below the floor nothing counts, however sudden.
  input.Configure(ATTACK_MODE_SNAP, kAttackThreshold);
  for (int b = 0; b < 200; ++b) {
    Feed(&input, 0.0f);
  }
  CHECK(!Feed(&input, 0.001f));
  // Hits every 2.5 ms count once: the level stays up between them.
  input.Configure(ATTACK_MODE_SNAP, kAttackThreshold);
  for (int b = 0; b < 200; ++b) {
    Feed(&input, 0.001f);
  }
  onsets = 0;
  for (int b = 0; b < 400; ++b) {
    onsets += Feed(&input, b % 10 == 0 ? 0.5f : 0.001f);
  }
  CHECK(onsets == 1);
  // Hits 100 ms apart count each time.
  for (int b = 0; b < 400; ++b) {
    Feed(&input, 0.001f);
  }
  onsets = 0;
  for (int b = 0; b < 2000; ++b) {
    onsets += Feed(&input, b % 400 == 0 ? 0.5f : 0.001f);
  }
  CHECK(onsets == 5);
  // An unpatched channel is ignored.
  input.Configure(ATTACK_MODE_SNAP, kAttackThreshold);
  for (int b = 0; b < 200; ++b) {
    Feed(&input, 0.01f);
  }
  CodecFrame in[kBlockSize];
  for (size_t i = 0; i < kBlockSize; ++i) {
    in[i].l = 0;
    in[i].r = int32_t(0.5f * 2147483647.0f);
  }
  CHECK(!input.Process(in, kBlockSize, kScale, 0.0f, kRate, false, true));
}

void TestAttackSnap() {
  test_checker.Start("TestAttackSnap");
  AttackDetector detector;
  detector.Init(kRate);
  detector.Configure(ATTACK_MODE_SNAP);
  const uint32_t capacity = 192000;
  // Marks at clock 1000 and 30000, recording up to clock 40000.
  uint32_t clock = 0;
  while (clock < 40000) {
    detector.SetOnset(clock == 996 || clock == 29999 - 29999 % kBlockSize);
    detector.BeginBlock(0, capacity, true, false);
    detector.EndBlock(kBlockSize);
    clock += kBlockSize;
  }
  CHECK(detector.count_ == 2);
  const float newer = float(clock - detector.marks_[1]);
  const float older = float(clock - detector.marks_[0]);
  detector.start_ptr_ = 0;
  // A start just after an attack moves back onto it.
  CHECK(detector.Snap(newer - 100.0f, capacity, 0) == newer);
  CHECK(detector.Snap(older - 4000.0f, capacity, 0) == older);
  // An attack more than 100 ms earlier is too far.
  CHECK(detector.Snap(newer - 5000.0f, capacity, 0) == newer - 5000.0f);
  // Starts older than every mark, or out of range, stay.
  CHECK(detector.Snap(older + 10.0f, capacity, 0) == older + 10.0f);
  CHECK(detector.Snap(-1.0f, capacity, 0) == -1.0f);
  // While frozen, no new marks; outside granular playback, none at all.
  detector.SetOnset(true);
  detector.BeginBlock(0, capacity, true, true);
  CHECK(detector.count_ == 2);
  detector.BeginBlock(0, capacity, false, false);
  CHECK(detector.count_ == 0);
  // Marks the recording has overwritten are forgotten.
  detector.Configure(ATTACK_MODE_SNAP);
  detector.SetOnset(true);
  detector.BeginBlock(0, 1200, true, false);
  detector.EndBlock(1200);
  detector.BeginBlock(0, 1200, true, false);
  CHECK(detector.count_ == 0);
}

// Off leaves the grains alone; Snap moves them, and stays finite.
void TestAttackProcessor() {
  test_checker.Start("TestAttackProcessor");
  static Processor processor;
  static uint8_t banks[3][262144];
  static uint16_t reverb[32768];
  uint8_t* buffers[] = { banks[0], banks[1], banks[2] };
  uint32_t hashes[2];
  for (int mode = 0; mode < 2; ++mode) {
    memset(static_cast<void*>(&processor), 0, sizeof(processor));
    memset(banks, 0, sizeof(banks));
    memset(reverb, 0, sizeof(reverb));
    stmlib::Random::Seed(0x12345678);
    processor.Init(buffers, reverb);
    processor.Configure(Processor::MEDIA_DIGITAL, false);
    processor.SetAttacks(mode, kAttackThreshold);
    Parameters p;
    memset(&p, 0, sizeof(p));
    p.time.base = 0.05f;
    p.size.base = 0.3f;
    p.shape.base = 0.3f;
    ModulatedParameter density;
    density.base = 0.95f;
    density.cv = 0.0f;
    Processor::MixParameters mix;
    mix.feedback = 0.0f;
    mix.dry_wet = 1.0f;
    mix.reverb_amount = 0.0f;
    mix.output_gate = false;
    uint32_t hash = 2166136261U;
    double energy = 0.0;
    for (int block = 0; block < 8000; ++block) {
      FloatFrame in[kBlockSize], out[kBlockSize];
      bool freeze[kBlockSize] = { };
      CodecFrame codec[kBlockSize];
      for (size_t i = 0; i < kBlockSize; ++i) {
        // A click every 0.25 s on a quiet tone.
        const int n = block * kBlockSize + int(i);
        const float x = (n % 12000 < 24 ? 0.5f : 0.0f) +
            0.01f * sinf(float(n) * 0.05f);
        in[i].l = in[i].r = x;
        codec[i].l = codec[i].r = int32_t(x * 2147483647.0f);
      }
      if (processor.attack_input()->mode()) {
        processor.SetAttackOnset(processor.attack_input()->Process(
            codec, kBlockSize, kScale, 0.0f, kRate, false, false));
      }
      processor.Process(SEED_BUTTON_STATE_ON, NULL, freeze,
          density, p, mix, in, out, kBlockSize);
      for (size_t i = 0; i < kBlockSize; ++i) {
        CHECK(std::isfinite(out[i].l) && std::isfinite(out[i].r));
      }
      if (block >= 6000) {
        for (size_t i = 0; i < kBlockSize; ++i) {
          energy += out[i].l * out[i].l;
        }
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(out);
        for (size_t i = 0; i < sizeof(out); ++i) {
          hash = (hash ^ bytes[i]) * 16777619U;
        }
      }
    }
    hashes[mode] = hash;
    CHECK(energy > 0.01);
    if (mode) {
      CHECK(processor.processor_digital_.attacks_.count_ > 0);
    }
  }
  printf("Attacks off %08x, snap %08x\n", hashes[0], hashes[1]);
  CHECK(hashes[0] != hashes[1]);
}

int main() {
  setbuf(stdout, NULL);
  TestAttackInput();
  TestAttackSnap();
  TestAttackProcessor();
  return test_checker.failures() ? 1 : 0;
}
