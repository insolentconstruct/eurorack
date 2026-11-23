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
// particle_burst: extremes, output bounds and seeded reset replay.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "stmlib/utils/random.h"

#include "beads/blob/models/particle_burst_engine.h"

namespace {

const size_t kBlockSize = 24;
const size_t kReplayBlocks = 256;
const float kGuard = 12345.0f;

void Poison(float* buffer) {
  buffer[0] = kGuard;
  buffer[kBlockSize + 1] = kGuard;
  memset(buffer + 1, 0xff, kBlockSize * sizeof(float));
}

bool Check(const float* buffer, size_t size) {
  if (buffer[0] != kGuard || buffer[kBlockSize + 1] != kGuard) {
    return false;
  }
  for (size_t i = 0; i < size; ++i) {
    if (!std::isfinite(buffer[i + 1])) {
      return false;
    }
  }
  for (size_t i = size; i < kBlockSize; ++i) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(buffer + i + 1);
    for (size_t j = 0; j < sizeof(float); ++j) {
      if (bytes[j] != 0xff) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

int main() {
  plaits::ParticleBurst engine;
  stmlib::BufferAllocator allocator(NULL, 0);
  engine.Init(&allocator);
  plaits::EngineParameters parameters;
  memset(&parameters, 0, sizeof(parameters));
  float out[kBlockSize + 2];
  float aux[kBlockSize + 2];
  bool already_enveloped = true;
  const size_t sizes[] = { 1, 12, 24 };
  for (int note = -24; note <= 156; note += 12) {
    parameters.note = note;
    for (int controls = 0; controls < 27; ++controls) {
      parameters.harmonics = (controls % 3) * 0.5f;
      parameters.timbre = ((controls / 3) % 3) * 0.5f;
      parameters.morph = (controls / 9) * 0.5f;
      parameters.accent = controls & 1 ? 1.0f : 0.0f;
      for (size_t block_size = 0; block_size < 3; ++block_size) {
        stmlib::Random::Seed(0x21);
        engine.Reset();
        for (int block = 0; block < 64; ++block) {
          parameters.trigger = block % 19 == 0
              ? plaits::TRIGGER_RISING_EDGE : plaits::TRIGGER_HIGH;
          if (block >= 32) {
            parameters.trigger = plaits::TRIGGER_UNPATCHED;
          }
          Poison(out);
          Poison(aux);
          const size_t size = sizes[block_size];
          engine.Render(parameters, out + 1, aux + 1, size,
              &already_enveloped);
          if (!Check(out, size) || !Check(aux, size)) {
            printf("ERROR: ParticleBurst: invalid output\n");
            return 1;
          }
        }
      }
    }
  }
  float reset_out[kReplayBlocks][kBlockSize];
  float reset_aux[kReplayBlocks][kBlockSize];
  parameters.note = 60.0f;
  parameters.harmonics = 0.8f;
  parameters.timbre = 0.7f;
  parameters.morph = 0.8f;
  parameters.accent = 0.8f;
  for (int unpatched = 0; unpatched < 2; ++unpatched) {
    stmlib::Random::Seed(0x21);
    engine.Reset();
    bool audible_out = false;
    bool audible_aux = false;
    for (size_t block = 0; block < kReplayBlocks; ++block) {
      parameters.trigger = unpatched ? plaits::TRIGGER_UNPATCHED
          : (block == 0 ? plaits::TRIGGER_RISING_EDGE : plaits::TRIGGER_HIGH);
      engine.Render(parameters, reset_out[block], reset_aux[block], kBlockSize,
          &already_enveloped);
      for (size_t i = 0; i < kBlockSize; ++i) {
        audible_out |= reset_out[block][i] != 0.0f;
        audible_aux |= reset_aux[block][i] != 0.0f;
      }
    }
    if (!audible_out || !audible_aux) {
      printf("ERROR: ParticleBurst: reset replay was silent\n");
      return 1;
    }
    for (size_t block = 0; block < 32; ++block) {
      engine.Render(parameters, out + 1, aux + 1, kBlockSize,
          &already_enveloped);
    }
    stmlib::Random::Seed(0x21);
    engine.Reset();
    for (size_t block = 0; block < kReplayBlocks; ++block) {
      parameters.trigger = unpatched ? plaits::TRIGGER_UNPATCHED
          : (block == 0 ? plaits::TRIGGER_RISING_EDGE : plaits::TRIGGER_HIGH);
      engine.Render(parameters, out + 1, aux + 1, kBlockSize,
          &already_enveloped);
      if (memcmp(reset_out[block], out + 1, kBlockSize * sizeof(float)) ||
          memcmp(reset_aux[block], aux + 1, kBlockSize * sizeof(float))) {
        printf("ERROR: ParticleBurst: warm reset changed OUT or AUX\n");
        return 1;
      }
    }
  }
  printf("ParticleBurst: extremes, bounds and reset replay OK\n");
  return 0;
}
