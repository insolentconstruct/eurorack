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
// SawComb control extremes, guarded outputs and reset replay.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "stmlib/utils/random.h"

#include "beads/blob/models/saw_comb_engine.h"

struct GuardedOutput {
  uint32_t before;
  float samples[24];
  uint32_t after;
};

bool Render(plaits::SawComb* engine,
    const plaits::EngineParameters& parameters,
    GuardedOutput* out,
    GuardedOutput* aux) {
  out->before = 0x12579bdf;
  aux->before = 0x12579bdf;
  out->after = 0xfdb97521;
  aux->after = 0xfdb97521;
  memset(out->samples, 0xff, sizeof(out->samples));
  memset(aux->samples, 0xff, sizeof(aux->samples));
  bool already_enveloped = true;
  engine->Render(parameters, out->samples, aux->samples, 24,
      &already_enveloped);
  if (already_enveloped || out->before != 0x12579bdf ||
      aux->before != 0x12579bdf || out->after != 0xfdb97521 ||
      aux->after != 0xfdb97521) {
    printf("ERROR: SawComb: output guard or envelope flag\n");
    return false;
  }
  for (size_t i = 0; i < 24; ++i) {
    if (!std::isfinite(out->samples[i]) ||
        !std::isfinite(aux->samples[i])) {
      printf("ERROR: SawComb: non-finite output\n");
      return false;
    }
  }
  return true;
}

int main() {
  plaits::SawComb engine;
  struct {
    uint32_t before;
    int16_t samples[plaits::kSawCombDelaySize];
    uint32_t after;
  } arena;
  arena.before = 0x12579bdf;
  arena.after = 0xfdb97521;
  memset(arena.samples, 0xa5, sizeof(arena.samples));
  stmlib::BufferAllocator allocator(arena.samples, sizeof(arena.samples));
  engine.Init(&allocator);
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
      stmlib::Random::Seed(0x21);
      for (int block = 0; block < 256; ++block) {
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
  if (allocator.free() != 0 || arena.before != 0x12579bdf ||
      arena.after != 0xfdb97521) {
    printf("ERROR: SawComb: arena bounds or declared size\n");
    return 1;
  }

  float reset_out[32][24];
  float reset_aux[32][24];
  parameters.note = 60.0f;
  parameters.timbre = 0.7f;
  parameters.morph = 0.5f;
  for (int model = 0; model < 3; ++model) {
    parameters.harmonics = model * 0.5f;
    engine.Reset();
    stmlib::Random::Seed(0x21);
    float energy = 0.0f;
    for (size_t block = 0; block < 32; ++block) {
      parameters.trigger = block % 7 == 0 ? plaits::TRIGGER_RISING_EDGE : 0;
      if (!Render(&engine, parameters, &out, &aux)) {
        return 1;
      }
      memcpy(reset_out[block], out.samples, sizeof(out.samples));
      memcpy(reset_aux[block], aux.samples, sizeof(aux.samples));
      for (size_t i = 0; i < 24; ++i) {
        energy += out.samples[i] * out.samples[i];
        energy += aux.samples[i] * aux.samples[i];
      }
    }
    if (energy == 0.0f) {
      printf("ERROR: SawComb: representative output is silent\n");
      return 1;
    }
    for (size_t block = 0; block < 256; ++block) {
      if (!Render(&engine, parameters, &out, &aux)) {
        return 1;
      }
    }
    engine.Reset();
    stmlib::Random::Seed(0x21);
    for (size_t block = 0; block < 32; ++block) {
      parameters.trigger = 0;
      if (!Render(&engine, parameters, &out, &aux)) {
        return 1;
      }
      if (memcmp(reset_out[block], out.samples, sizeof(out.samples)) ||
          memcmp(reset_aux[block], aux.samples, sizeof(aux.samples))) {
        printf("ERROR: SawComb: warm reset changed OUT or AUX\n");
        return 1;
      }
    }
  }
  stmlib::BufferAllocator short_allocator(arena.samples,
      sizeof(arena.samples) - 1);
  engine.Init(&short_allocator);
  if (!Render(&engine, parameters, &out, &aux)) {
    return 1;
  }
  for (size_t i = 0; i < 24; ++i) {
    if (out.samples[i] != 0.0f || aux.samples[i] != 0.0f) {
      printf("ERROR: SawComb: short arena did not produce silence\n");
      return 1;
    }
  }
  printf("SawComb: control extremes, guards and reset OK\n");
  return 0;
}
