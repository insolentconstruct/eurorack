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
// Fluted's control extremes, poisoned outputs, arena guards and warm reset.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "stmlib/utils/random.h"

#include "beads/blob/models/fluted_engine.h"

namespace {

const size_t kBlockSize = 24;
const size_t kReplayBlocks = 256;
const size_t kArenaSize = 5120;
uint8_t arena[kArenaSize + 64] __attribute__((aligned(8)));
float replay_out[kReplayBlocks][kBlockSize];
float replay_aux[kReplayBlocks][kBlockSize];

bool Finite(const float* out, const float* aux) {
  for (size_t i = 0; i < kBlockSize; ++i) {
    if (!std::isfinite(out[i]) || !std::isfinite(aux[i])) {
      return false;
    }
  }
  for (size_t i = 0; i < 32; ++i) {
    if (arena[i] != 0xa5 || arena[kArenaSize + 32 + i] != 0xa5) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  memset(arena, 0xa5, sizeof(arena));
  plaits::Fluted engine;
  stmlib::BufferAllocator allocator(arena + 32, kArenaSize);
  stmlib::Random::Seed(0x21);
  engine.Init(&allocator);
  if (allocator.free() != 0) {
    printf("ERROR: Fluted: arena declaration does not match allocations\n");
    return 1;
  }
  plaits::EngineParameters parameters;
  memset(&parameters, 0, sizeof(parameters));
  parameters.accent = 0.8f;
  float out[kBlockSize];
  float aux[kBlockSize];
  bool already_enveloped = false;
  parameters.note = 60.0f;
  parameters.harmonics = 0.5f;
  parameters.timbre = 0.7f;
  parameters.morph = 0.5f;
  parameters.trigger = plaits::TRIGGER_UNPATCHED;
  memset(out, 0xff, sizeof(out));
  memset(aux, 0xff, sizeof(aux));
  engine.Render(parameters, out, aux, kBlockSize, &already_enveloped);
  if (!Finite(out, aux)) {
    printf("ERROR: Fluted: first render after Init poisoned output or arena\n");
    return 1;
  }
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
        engine.Render(parameters, out, aux, kBlockSize, &already_enveloped);
        if (!Finite(out, aux) || already_enveloped != false) {
          printf("ERROR: Fluted: output, arena or envelope at control extreme\n");
          return 1;
        }
      }
    }
  }
  parameters.note = 45.0f;
  parameters.harmonics = 0.5f;
  parameters.timbre = 0.9f;
  parameters.morph = 0.5f;
  for (int unpatched = 0; unpatched < 2; ++unpatched) {
    stmlib::Random::Seed(0x21);
    engine.Reset();
    bool nonzero_out = false;
    bool nonzero_aux = false;
    for (size_t block = 0; block < kReplayBlocks; ++block) {
      parameters.trigger = unpatched ? plaits::TRIGGER_UNPATCHED :
          (block == 0 ? plaits::TRIGGER_RISING_EDGE : plaits::TRIGGER_HIGH);
      engine.Render(parameters, replay_out[block], replay_aux[block],
          kBlockSize, &already_enveloped);
      for (size_t i = 0; i < kBlockSize; ++i) {
        nonzero_out |= replay_out[block][i] != 0.0f;
        nonzero_aux |= replay_aux[block][i] != 0.0f;
      }
    }
    if (!nonzero_out || !nonzero_aux) {
      printf("ERROR: Fluted: replay did not exercise both outputs\n");
      return 1;
    }
    stmlib::Random::Seed(0x21);
    engine.Reset();
    for (size_t block = 0; block < kReplayBlocks; ++block) {
      parameters.trigger = unpatched ? plaits::TRIGGER_UNPATCHED :
          (block == 0 ? plaits::TRIGGER_RISING_EDGE : plaits::TRIGGER_HIGH);
      memset(out, 0xff, sizeof(out));
      memset(aux, 0xff, sizeof(aux));
      engine.Render(parameters, out, aux, kBlockSize, &already_enveloped);
      if (!Finite(out, aux) ||
          memcmp(replay_out[block], out, sizeof(out)) ||
          memcmp(replay_aux[block], aux, sizeof(aux))) {
        printf("ERROR: Fluted: warm reset changed OUT or AUX\n");
        return 1;
      }
    }
  }
  // A strike keeps filters, interpolation and noise history alive.
  parameters.trigger = plaits::TRIGGER_RISING_EDGE;
  engine.Render(parameters, out, aux, kBlockSize, &already_enveloped);
  if (memcmp(replay_out[0], out, sizeof(out)) == 0 &&
      memcmp(replay_aux[0], aux, sizeof(aux)) == 0) {
    printf("ERROR: Fluted: strike lost retained state\n");
    return 1;
  }
  plaits::Fluted empty;
  stmlib::BufferAllocator empty_allocator(NULL, 0);
  empty.Init(&empty_allocator);
  memset(out, 0xff, sizeof(out));
  memset(aux, 0xff, sizeof(aux));
  empty.Render(parameters, out, aux, kBlockSize, &already_enveloped);
  for (size_t i = 0; i < kBlockSize; ++i) {
    if (out[i] != 0.0f || aux[i] != 0.0f) {
      printf("ERROR: Fluted: exhausted arena was not silent\n");
      return 1;
    }
  }

  printf("Fluted: extremes, arena and reset replay OK\n");
  return 0;
}
