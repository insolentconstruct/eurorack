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
// ScaleStack's control extremes, output guards and triggered warm-reset replay.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "beads/blob/models/scale_stack_engine.h"
#include "beads/test/scale_voices_stubs.h"

struct GuardedOutput {
  uint32_t before;
  float samples[24];
  uint32_t after;
};

bool Render(plaits::ScaleStack* engine,
    const plaits::EngineParameters& parameters,
    GuardedOutput* out,
    GuardedOutput* aux) {
  out->before = 0x12579bdf;
  aux->before = 0x12579bdf;
  out->after = 0xfdb97521;
  aux->after = 0xfdb97521;
  memset(out->samples, 0xff, sizeof(out->samples));
  memset(aux->samples, 0xff, sizeof(aux->samples));
  bool already_enveloped = false;
  engine->Render(parameters, out->samples, aux->samples, 24,
      &already_enveloped);
  if (already_enveloped || out->before != 0x12579bdf ||
      aux->before != 0x12579bdf || out->after != 0xfdb97521 ||
      aux->after != 0xfdb97521) {
    printf("ERROR: ScaleStack: output guard or envelope flag\n");
    return false;
  }
  for (size_t i = 0; i < 24; ++i) {
    if (!std::isfinite(out->samples[i]) ||
        !std::isfinite(aux->samples[i])) {
      printf("ERROR: ScaleStack: non-finite output\n");
      return false;
    }
  }
  return true;
}

int main() {
  plaits::ScaleStack engine;
  stmlib::BufferAllocator allocator(NULL, 0);
  beads::PlaitsBlobLibrary scales;
  memset(&scales, 0, sizeof(scales));
  scales.exports[beads::SCALE_VOICES_NUM_SCALES] =
      &plaits::kScaleVoicesNumScales;
  engine.Init(&scales, &allocator);
  plaits::EngineParameters parameters;
  memset(&parameters, 0, sizeof(parameters));
  parameters.accent = 0.8f;
  GuardedOutput out;
  GuardedOutput aux;
  for (int note = -24; note <= 156; note += 12) {
    parameters.note = note;
    for (int control = 0; control < 27; ++control) {
      parameters.harmonics = (control % 3) * 0.5f;
      parameters.timbre = ((control / 3) % 3) * 0.5f;
      parameters.morph = (control / 9) * 0.5f;
      engine.Reset();
      for (int block = 0; block < 64; ++block) {
        parameters.trigger = block % 7 == 0 ? plaits::TRIGGER_RISING_EDGE : 0;
        if (block % 16 < 8) {
          parameters.trigger |= plaits::TRIGGER_HIGH;
        }
        if (!Render(&engine, parameters, &out, &aux)) {
          return 1;
        }
      }
    }
  }

  parameters.note = 36.0f;
  parameters.harmonics = 0.2f;
  parameters.timbre = 0.7f;
  parameters.morph = 0.5f;
  float reset_out[16][24];
  float reset_aux[16][24];
  float energy = 0.0f;
  engine.Reset();
  for (size_t block = 0; block < 16; ++block) {
    parameters.trigger = block % 7 == 0 ? plaits::TRIGGER_RISING_EDGE : 0;
    if (!Render(&engine, parameters, &out, &aux)) {
      return 1;
    }
    memcpy(reset_out[block], out.samples, sizeof(out.samples));
    memcpy(reset_aux[block], aux.samples, sizeof(aux.samples));
    for (size_t i = 0; i < 24; ++i) {
      energy += out.samples[i] * out.samples[i];
    }
  }
  if (energy == 0.0f) {
    printf("ERROR: ScaleStack: rising trigger produced silence\n");
    return 1;
  }
  for (size_t block = 0; block < 64; ++block) {
    if (!Render(&engine, parameters, &out, &aux)) {
      return 1;
    }
  }
  engine.Reset();
  for (size_t block = 0; block < 16; ++block) {
    parameters.trigger = block % 7 == 0 ? plaits::TRIGGER_RISING_EDGE : 0;
    if (!Render(&engine, parameters, &out, &aux)) {
      return 1;
    }
    if (memcmp(reset_out[block], out.samples, sizeof(out.samples)) ||
        memcmp(reset_aux[block], aux.samples, sizeof(aux.samples))) {
      printf("ERROR: ScaleStack: warm reset changed OUT or AUX\n");
      return 1;
    }
  }
  printf("ScaleStack: control extremes, guards and reset OK\n");
  return 0;
}
