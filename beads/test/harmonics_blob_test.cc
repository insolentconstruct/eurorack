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
// Harmonics's control extremes, reset determinism and poisoned outputs under ASan.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "beads/blob/models/harmonics_engine.h"

int main() {
  plaits::Harmonics engine;
  stmlib::BufferAllocator allocator(NULL, 0);
  engine.Init(&allocator);
  plaits::EngineParameters parameters;
  memset(&parameters, 0, sizeof(parameters));
  parameters.accent = 0.8f;
  float out[24];
  float aux[24];
  float first[24];
  float first_aux[24];
  bool already_enveloped = false;
  for (int note = -24; note <= 156; note += 12) {
    parameters.note = note;
    for (int control = 0; control < 8; ++control) {
      parameters.harmonics = control & 1 ? 1.0f : 0.0f;
      parameters.timbre = control & 2 ? 1.0f : 0.0f;
      parameters.morph = control & 4 ? 1.0f : 0.0f;
      engine.Reset();
      const float target_timbre = parameters.timbre;
      for (int block = 0; block < 64; ++block) {
        parameters.timbre = block % 8 < 4 ? target_timbre : 0.5f;
        parameters.trigger = block % 7 == 0 ? plaits::TRIGGER_RISING_EDGE : 0;
        if (block % 16 < 8) {
          parameters.trigger |= plaits::TRIGGER_HIGH;
        }
        memset(out, 0xff, sizeof(out));
        memset(aux, 0xff, sizeof(aux));
        engine.Render(parameters, out, aux, 24, &already_enveloped);
        if (already_enveloped) {
          printf("ERROR: Harmonics: unexpected envelope flag\n");
          return 1;
        }
        for (size_t i = 0; i < 24; ++i) {
          if (!std::isfinite(out[i]) || !std::isfinite(aux[i])) {
            printf("ERROR: Harmonics: non-finite output\n");
            return 1;
          }
        }
        if (block == 0) {
          memcpy(first, out, sizeof(first));
          memcpy(first_aux, aux, sizeof(first_aux));
          engine.Reset();
          engine.Render(parameters, out, aux, 24, &already_enveloped);
          if (memcmp(first, out, sizeof(first)) ||
              memcmp(first_aux, aux, sizeof(first_aux))) {
            printf("ERROR: Harmonics: reset changed output\n");
            return 1;
          }
        }
      }
    }
  }
  float reset_out[4][24];
  float reset_aux[4][24];
  parameters.note = 60.0f;
  parameters.harmonics = 0.2f;
  parameters.timbre = 0.7f;
  parameters.morph = 0.8f;
  parameters.trigger = plaits::TRIGGER_HIGH;
  engine.Reset();
  for (size_t block = 0; block < 4; ++block) {
    engine.Render(parameters, reset_out[block], reset_aux[block], 24,
        &already_enveloped);
  }
  for (size_t block = 0; block < 64; ++block) {
    engine.Render(parameters, out, aux, 24, &already_enveloped);
  }
  engine.Reset();
  for (size_t block = 0; block < 4; ++block) {
    engine.Render(parameters, out, aux, 24, &already_enveloped);
    if (memcmp(reset_out[block], out, sizeof(out)) ||
        memcmp(reset_aux[block], aux, sizeof(aux))) {
      printf("ERROR: Harmonics: warm reset changed OUT or AUX\n");
      return 1;
    }
  }
  printf("Harmonics: control extremes and reset OK\n");
  return 0;
}
