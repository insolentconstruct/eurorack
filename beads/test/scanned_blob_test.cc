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
// Scanned's control extremes, reset determinism and poisoned outputs under ASan.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "stmlib/utils/random.h"

#include "beads/blob/models/scanned_engine.h"

int main() {
  plaits::Scanned engine;
  stmlib::BufferAllocator allocator(NULL, 0);
  engine.Init(&allocator);
  plaits::EngineParameters parameters;
  memset(&parameters, 0, sizeof(parameters));
  parameters.accent = 0.8f;
  float out[24];
  float aux[24];
  float first[24];
  bool already_enveloped = false;
  for (int note = -24; note <= 156; note += 12) {
    parameters.note = note;
    for (int control = 0; control < 8; ++control) {
      parameters.harmonics = control & 1 ? 1.0f : 0.0f;
      parameters.timbre = control & 2 ? 1.0f : 0.0f;
      parameters.morph = control & 4 ? 1.0f : 0.0f;
      stmlib::Random::Seed(0x21);
      engine.Reset();
      for (int block = 0; block < 64; ++block) {
        parameters.trigger = block % 7 == 0 ? plaits::TRIGGER_RISING_EDGE : 0;
        if (block % 16 < 8) {
          parameters.trigger |= plaits::TRIGGER_HIGH;
        }
        if (block >= 32) {
          parameters.trigger = plaits::TRIGGER_UNPATCHED;
        }
        memset(out, 0xff, sizeof(out));
        memset(aux, 0xff, sizeof(aux));
        engine.Render(parameters, out, aux, 24, &already_enveloped);
        for (size_t i = 0; i < 24; ++i) {
          if (!std::isfinite(out[i]) || !std::isfinite(aux[i])) {
            printf("ERROR: Scanned: non-finite output\n");
            return 1;
          }
        }
        if (block == 0) {
          memcpy(first, out, sizeof(first));
          stmlib::Random::Seed(0x21);
          engine.Reset();
          engine.Render(parameters, out, aux, 24, &already_enveloped);
          if (memcmp(first, out, sizeof(first))) {
            printf("ERROR: Scanned: reset changed output\n");
            return 1;
          }
        }
      }
    }
  }
  const size_t kReplayBlocks = 128;
  float reset_out[kReplayBlocks][24];
  float reset_aux[kReplayBlocks][24];
  parameters.note = 60.0f;
  parameters.harmonics = 0.2f;
  parameters.timbre = 0.7f;
  parameters.morph = 0.8f;
  for (int driven = 0; driven < 2; ++driven) {
    parameters.trigger = driven ? plaits::TRIGGER_UNPATCHED : plaits::TRIGGER_HIGH;
    stmlib::Random::Seed(0x21);
    engine.Reset();
    bool nonzero = false;
    for (size_t block = 0; block < kReplayBlocks; ++block) {
      engine.Render(parameters, reset_out[block], reset_aux[block], 24,
          &already_enveloped);
      for (size_t i = 0; i < 24; ++i) {
        nonzero |= reset_out[block][i] != 0.0f || reset_aux[block][i] != 0.0f;
      }
    }
    if (!nonzero) {
      printf("ERROR: Scanned: reset replay was silent\n");
      return 1;
    }
    for (size_t block = 0; block < 64; ++block) {
      engine.Render(parameters, out, aux, 24, &already_enveloped);
    }
    stmlib::Random::Seed(0x21);
    engine.Reset();
    for (size_t block = 0; block < kReplayBlocks; ++block) {
      engine.Render(parameters, out, aux, 24, &already_enveloped);
      if (memcmp(reset_out[block], out, sizeof(out)) ||
          memcmp(reset_aux[block], aux, sizeof(aux))) {
        printf("ERROR: Scanned: warm reset changed OUT or AUX\n");
        return 1;
      }
    }
  }
  // The last replay is driven; the same strike without driving must diverge.
  parameters.trigger = plaits::TRIGGER_HIGH;
  stmlib::Random::Seed(0x21);
  engine.Reset();
  bool different = false;
  for (size_t block = 0; block < kReplayBlocks; ++block) {
    engine.Render(parameters, out, aux, 24, &already_enveloped);
    different |= memcmp(reset_out[block], out, sizeof(out)) != 0 ||
        memcmp(reset_aux[block], aux, sizeof(aux)) != 0;
  }
  if (!different) {
    printf("ERROR: Scanned: unpatched driving did not change OUT or AUX\n");
    return 1;
  }
  printf("Scanned: control extremes, reset and random driving OK\n");
  return 0;
}
