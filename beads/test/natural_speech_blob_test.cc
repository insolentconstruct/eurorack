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
// Natural Speech control extremes, guarded outputs and fresh-start replay.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "stmlib/utils/random.h"

#include "beads/blob/models/natural_speech_engine.h"

struct GuardedOutput {
  uint32_t before;
  float samples[24];
  uint32_t after;
};

struct GuardedEngine {
  uint32_t before;
  plaits::NaturalSpeech engine;
  uint32_t after;
};

bool Render(GuardedEngine* state,
    const plaits::EngineParameters& parameters,
    GuardedOutput* out,
    GuardedOutput* aux) {
  out->before = aux->before = 0x12579bdf;
  out->after = aux->after = 0xfdb97521;
  memset(out->samples, 0xff, sizeof(out->samples));
  memset(aux->samples, 0xff, sizeof(aux->samples));
  bool already_enveloped = parameters.trigger & plaits::TRIGGER_UNPATCHED;
  state->engine.Render(parameters, out->samples, aux->samples, 24,
      &already_enveloped);
  if (already_enveloped != !(parameters.trigger & plaits::TRIGGER_UNPATCHED) ||
      out->before != 0x12579bdf || aux->before != 0x12579bdf ||
      out->after != 0xfdb97521 || aux->after != 0xfdb97521 ||
      state->before != 0x12579bdf || state->after != 0xfdb97521) {
    printf("ERROR: Natural Speech: state/output guard or envelope flag\n");
    return false;
  }
  for (size_t i = 0; i < 24; ++i) {
    if (!std::isfinite(out->samples[i]) || !std::isfinite(aux->samples[i])) {
      printf("ERROR: Natural Speech: non-finite output\n");
      return false;
    }
  }
  return true;
}

int main() {
  GuardedEngine state;
  state.before = 0x12579bdf;
  state.after = 0xfdb97521;
  uint8_t arena[16];
  memset(arena, 0xa5, sizeof(arena));
  stmlib::BufferAllocator allocator(arena, sizeof(arena));
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
      state.engine.Init(&allocator);
      stmlib::Random::Seed(0x21);
      for (int block = 0; block < 128; ++block) {
        parameters.trigger = block < 64 ? plaits::TRIGGER_UNPATCHED :
            block % 17 == 0 ? plaits::TRIGGER_RISING_EDGE : 0;
        if (!Render(&state, parameters, &out, &aux)) {
          return 1;
        }
      }
    }
  }
  if (allocator.free() != sizeof(arena)) {
    printf("ERROR: Natural Speech: unexpected arena allocation\n");
    return 1;
  }
  for (size_t i = 0; i < sizeof(arena); ++i) {
    if (arena[i] != 0xa5) {
      printf("ERROR: Natural Speech: arena changed\n");
      return 1;
    }
  }

  float replay_out[128][24];
  float replay_aux[128][24];
  parameters.note = 60.0f;
  parameters.timbre = 0.7f;
  parameters.morph = 0.5f;
  for (int bank = 0; bank < 2; ++bank) {
    parameters.harmonics = bank;
    for (int free_running = 0; free_running < 2; ++free_running) {
      state.engine.Init(&allocator);
      stmlib::Random::Seed(0x21);
      float energy = 0.0f;
      for (size_t block = 0; block < 128; ++block) {
        parameters.trigger = free_running ? plaits::TRIGGER_UNPATCHED :
            block == 0 ? plaits::TRIGGER_RISING_EDGE : 0;
        if (!Render(&state, parameters, &out, &aux)) {
          return 1;
        }
        memcpy(replay_out[block], out.samples, sizeof(out.samples));
        memcpy(replay_aux[block], aux.samples, sizeof(aux.samples));
        for (size_t i = 0; i < 24; ++i) {
          energy += out.samples[i] * out.samples[i];
          energy += aux.samples[i] * aux.samples[i];
        }
      }
      if (energy == 0.0f) {
        printf("ERROR: Natural Speech: representative output is silent\n");
        return 1;
      }
      for (size_t block = 0; block < 4096; ++block) {
        if (!Render(&state, parameters, &out, &aux)) {
          return 1;
        }
      }
      state.engine.Init(&allocator);
      stmlib::Random::Seed(0x21);
      for (size_t block = 0; block < 128; ++block) {
        parameters.trigger = free_running ? plaits::TRIGGER_UNPATCHED :
            block == 0 ? plaits::TRIGGER_RISING_EDGE : 0;
        if (!Render(&state, parameters, &out, &aux)) {
          return 1;
        }
        if (memcmp(replay_out[block], out.samples, sizeof(out.samples)) ||
            memcmp(replay_aux[block], aux.samples, sizeof(aux.samples))) {
          printf("ERROR: Natural Speech: fresh start changed OUT or AUX\n");
          return 1;
        }
      }
    }
  }
  printf("Natural Speech: control extremes, guards and fresh start OK\n");
  return 0;
}
