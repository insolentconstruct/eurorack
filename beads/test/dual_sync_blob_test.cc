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
// Control extremes, poisoned outputs and warm reset under ASan/UBSan.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "stmlib/utils/random.h"

#include "beads/blob/models/dual_sync_engine.h"

int main() {
  plaits::DualSync engine;
  stmlib::BufferAllocator allocator(NULL, 0);
  engine.Init(&allocator);
  plaits::EngineParameters parameters;
  memset(&parameters, 0, sizeof(parameters));
  float out[24];
  float aux[24];
  bool already_enveloped = false;
  for (int note = -24; note <= 156; note += 12) {
    parameters.note = note;
    for (int control = 0; control < 8; ++control) {
      parameters.harmonics = control & 1 ? 1.0f : 0.0f;
      parameters.timbre = control & 2 ? 1.0f : 0.0f;
      parameters.morph = control & 4 ? 1.0f : 0.0f;
      engine.Reset();
      stmlib::Random::Seed(0x21);
      for (int block = 0; block < 256; ++block) {
        if (block >= 128) {
          parameters.harmonics = (block - 128) / 127.0f;
        }
        parameters.trigger = block % 7 == 0
            ? plaits::TRIGGER_RISING_EDGE : plaits::TRIGGER_UNPATCHED;
        memset(out, 0xff, sizeof(out));
        memset(aux, 0xff, sizeof(aux));
        engine.Render(parameters, out, aux, 24, &already_enveloped);
        for (size_t i = 0; i < 24; ++i) {
          if (!std::isfinite(out[i]) || !std::isfinite(aux[i])) {
            printf("ERROR: DualSync: non-finite output\n");
            return 1;
          }
        }
        if (block == 127) {
          engine.Reset();
        }
      }
    }
  }
  printf("DualSync: control extremes and warm reset OK\n");
  return 0;
}
