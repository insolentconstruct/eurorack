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

#include "beads/blob/models/chords_supersaw_engine.h"
#include "beads/blob/plaits_blob.h"

extern "C" void* plaits_blob_init(
    void*, const beads::PlaitsBlobServices*, uint8_t*, size_t,
    const beads::PlaitsBlobLibrary* const*);
extern "C" void plaits_blob_reset(
    void*, const beads::PlaitsBlobServices*, uint32_t);
extern "C" void plaits_blob_render(
    void*, const beads::PlaitsBlobServices*, const plaits::EngineParameters*,
    float*, float*, size_t, bool*);

bool TestFreshReselection() {
  const size_t kBlockSize = 12;
  const size_t kBlocks = 256;
  uint8_t state[sizeof(plaits::ChordsSupersaw)]
      __attribute__((aligned(8)));
  plaits::ChordsSupersawEngine fresh;
  plaits::ChordsSupersawEngine retained;
  stmlib::BufferAllocator allocator(NULL, 0);
  retained.Init(&allocator);
  retained.Reset();
  plaits::EngineParameters parameters;
  memset(&parameters, 0, sizeof(parameters));
  parameters.note = 60.0f;
  parameters.harmonics = 0.7f;
  parameters.timbre = 0.8f;
  parameters.morph = 0.6f;
  parameters.trigger = plaits::TRIGGER_UNPATCHED;
  float out[kBlockSize];
  float aux[kBlockSize];
  float expected_out[kBlockSize];
  float expected_aux[kBlockSize];
  float retained_out[kBlockSize];
  float retained_aux[kBlockSize];
  bool already_enveloped = false;
  bool differs_from_retained = false;
  bool audible = false;
  for (size_t selection = 0; selection < 2; ++selection) {
    // A selected blob rebuilds zeroed state; Reset alone retains the phases.
    memset(state, 0, sizeof(state));
    void* object = plaits_blob_init(state, NULL, NULL, 0, NULL);
    plaits_blob_reset(object, NULL, 0);
    fresh.Init(&allocator);
    fresh.Reset();
    retained.Reset();
    for (size_t block = 0; block < kBlocks; ++block) {
      plaits_blob_render(object, NULL, &parameters, out, aux, kBlockSize,
          &already_enveloped);
      fresh.Render(parameters, expected_out, expected_aux, kBlockSize,
          &already_enveloped);
      retained.Render(parameters, retained_out, retained_aux, kBlockSize,
          &already_enveloped);
      if (memcmp(out, expected_out, sizeof(out)) ||
          memcmp(aux, expected_aux, sizeof(aux))) {
        printf("ERROR: Supersaw Chords: selection differs from fresh state\n");
        return false;
      }
      if (selection) {
        differs_from_retained |= memcmp(out, retained_out, sizeof(out)) ||
            memcmp(aux, retained_aux, sizeof(aux));
      }
      for (size_t i = 0; i < kBlockSize; ++i) {
        audible |= out[i] != 0.0f;
      }
    }
  }
  if (!audible || !differs_from_retained) {
    printf("ERROR: Supersaw Chords: fresh-state fixture is ineffective\n");
    return false;
  }
  return true;
}

int main() {
  if (!TestFreshReselection()) {
    return 1;
  }
  plaits::ChordsSupersaw engine;
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
            printf("ERROR: ChordsSupersaw: non-finite output\n");
            return 1;
          }
        }
        if (block == 127) {
          engine.Reset();
        }
      }
    }
  }
  printf("ChordsSupersaw: control extremes and warm reset OK\n");
  return 0;
}
