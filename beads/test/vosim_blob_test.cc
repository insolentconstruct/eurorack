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
// Vosim control extremes, guarded outputs and reset replay.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "stmlib/utils/random.h"

#include "beads/blob/models/vosim_engine.h"

struct GuardedOutput {
  uint32_t before;
  float samples[12];
  uint32_t after;
};

bool Render(plaits::Vosim* engine,
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
  engine->Render(parameters, out->samples, aux->samples, 12,
      &already_enveloped);
  if (already_enveloped || out->before != 0x12579bdf ||
      aux->before != 0x12579bdf || out->after != 0xfdb97521 ||
      aux->after != 0xfdb97521) {
    printf("ERROR: Vosim: output guard or envelope flag\n");
    return false;
  }
  for (size_t i = 0; i < 12; ++i) {
    if (!std::isfinite(out->samples[i]) ||
        !std::isfinite(aux->samples[i])) {
      printf("ERROR: Vosim: non-finite output\n");
      return false;
    }
  }
  return true;
}

int main() {
  plaits::Vosim engine;
  stmlib::BufferAllocator allocator(NULL, 0);
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

  parameters.note = 60.0f;
  parameters.harmonics = 0.7f;
  parameters.timbre = 0.5f;
  parameters.morph = 0.5f;
  parameters.trigger = 0;
  for (int interrupted = 0; interrupted <= 16; ++interrupted) {
    engine.Reset();
    for (int block = 0; block < interrupted; ++block) {
      if (!Render(&engine, parameters, &out, &aux)) {
        return 1;
      }
    }
    engine.Reset();
    plaits::Vosim fresh;
    fresh.Init(&allocator);
    GuardedOutput fresh_out;
    GuardedOutput fresh_aux;
    float energy = 0.0f;
    for (int block = 0; block < 32; ++block) {
      if (!Render(&engine, parameters, &out, &aux) ||
          !Render(&fresh, parameters, &fresh_out, &fresh_aux)) {
        return 1;
      }
      if (memcmp(out.samples, fresh_out.samples, sizeof(out.samples)) ||
          memcmp(aux.samples, fresh_aux.samples, sizeof(aux.samples))) {
        printf("ERROR: Vosim: interrupted preparation retained state\n");
        return 1;
      }
      for (size_t i = 0; i < 12; ++i) {
        if (block < 16 && (out.samples[i] != 0.0f ||
            aux.samples[i] != 0.0f)) {
          printf("ERROR: Vosim: preparation did not write silence\n");
          return 1;
        }
        energy += out.samples[i] * out.samples[i];
        energy += aux.samples[i] * aux.samples[i];
      }
      if (block == 16 && energy == 0.0f) {
        printf("ERROR: Vosim: preparation exceeded sixteen blocks\n");
        return 1;
      }
    }
  }

  engine.Reset();
  plaits::Vosim held;
  held.Init(&allocator);
  GuardedOutput held_out;
  GuardedOutput held_aux;
  for (int block = 0; block < 32; ++block) {
    plaits::EngineParameters moving = parameters;
    if (block > 0 && block < 16) {
      moving.note += block;
      moving.timbre = 1.0f - moving.timbre;
      moving.morph = block / 16.0f;
      moving.trigger = plaits::TRIGGER_RISING_EDGE;
    }
    if (!Render(&engine, moving, &out, &aux) ||
        !Render(&held, parameters, &held_out, &held_aux)) {
      return 1;
    }
    if (memcmp(out.samples, held_out.samples, sizeof(out.samples)) ||
        memcmp(aux.samples, held_aux.samples, sizeof(aux.samples))) {
      printf("ERROR: Vosim: preparation did not hold selection controls\n");
      return 1;
    }
  }

  float reset_out[32][12];
  float reset_aux[32][12];
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
      for (size_t i = 0; i < 12; ++i) {
        energy += out.samples[i] * out.samples[i];
        energy += aux.samples[i] * aux.samples[i];
      }
    }
    if (energy == 0.0f) {
      printf("ERROR: Vosim: representative output is silent\n");
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
      parameters.trigger = block % 7 == 0 ? plaits::TRIGGER_RISING_EDGE : 0;
      if (!Render(&engine, parameters, &out, &aux)) {
        return 1;
      }
      if (memcmp(reset_out[block], out.samples, sizeof(out.samples)) ||
          memcmp(reset_aux[block], aux.samples, sizeof(aux.samples))) {
        printf("ERROR: Vosim: warm reset changed OUT or AUX\n");
        return 1;
      }
    }
  }
  engine.Reset();
  stmlib::Random::Seed(0x21);
  for (size_t block = 0; block < 192; ++block) {
    parameters.timbre = ((block / 8) % 3) * 0.5f;
    parameters.trigger = block % 7 == 0 ? plaits::TRIGGER_RISING_EDGE : 0;
    if (!Render(&engine, parameters, &out, &aux)) {
      return 1;
    }
  }
  printf("Vosim: control extremes, guards, bounded preparation and fresh reset OK\n");
  return 0;
}
