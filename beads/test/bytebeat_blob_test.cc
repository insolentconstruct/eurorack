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
// Bytebeat: wrapper boundaries, intermediate controls and reset forwarding.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "beads/blob/plaits_blob.h"
#include "beads/blob/models/bytebeat_engine.h"

extern "C" {
extern const beads::PlaitsBlobHeader2 plaits_blob_header;
void* plaits_blob_init(void*, const beads::PlaitsBlobServices*, uint8_t*,
    size_t, const beads::PlaitsBlobLibrary* const*);
void plaits_blob_reset(void*, const beads::PlaitsBlobServices*, uint32_t);
void plaits_blob_render(void*, const beads::PlaitsBlobServices*,
    const plaits::EngineParameters*, float*, float*, size_t, bool*);
}

const uint32_t kCanary = 0x12345678;

struct State {
  uint32_t before;
  uint8_t bytes[sizeof(plaits::Bytebeat)] __attribute__((aligned(8)));
  uint32_t after;
};

struct Arena {
  uint32_t before;
  uint8_t bytes[192] __attribute__((aligned(8)));
  uint32_t after;
};

struct Output {
  uint32_t before;
  float samples[24];
  uint32_t after;
};

int main() {
  if (plaits_blob_header.first_id != 79 ||
      plaits_blob_header.state_size != sizeof(plaits::Bytebeat) ||
      plaits_blob_header.arena_size != 0 ||
      plaits_blob_header.out_gain != -0.653f ||
      plaits_blob_header.aux_gain != -0.653f ||
      plaits_blob_header.num_dependencies != 0) {
    printf("ERROR: Bytebeat: header metadata\n");
    return 1;
  }
  State state;
  state.before = state.after = kCanary;
  Arena arena;
  arena.before = arena.after = kCanary;
  Arena reference_arena;
  Output out;
  Output aux;
  out.before = out.after = aux.before = aux.after = kCanary;
  float reference_out[24];
  float reference_aux[24];
  plaits::Bytebeat reference;
  plaits::EngineParameters parameters;
  memset(&parameters, 0, sizeof(parameters));
  parameters.accent = 0.8f;
  const int notes[] = { -24, 60, 156 };
  const float controls[] = { 0.0f, 0.33333334f, 0.66666669f, 1.0f };
  for (size_t note = 0; note < 3; ++note) {
    parameters.note = notes[note];
    memset(state.bytes, 0, sizeof(state.bytes));
    void* object = plaits_blob_init(state.bytes, NULL,
        0 ? arena.bytes : NULL, 0, NULL);
    stmlib::BufferAllocator allocator(
        0 ? reference_arena.bytes : NULL, 0);
    reference.Init(&allocator);
    for (int control = 0; control < 64; ++control) {
      parameters.harmonics = controls[control % 4];
      parameters.timbre = controls[(control / 4) % 4];
      parameters.morph = controls[(control / 16) % 4];
      for (int block = 0; block < 1024; ++block) {
        parameters.trigger = block == 0 ? plaits::TRIGGER_RISING_EDGE : 0;
        if (block < 128) {
          parameters.trigger |= plaits::TRIGGER_HIGH;
        }
        if (block >= 512) {
          parameters.trigger = plaits::TRIGGER_UNPATCHED;
        }
        if (block == 512) {
          plaits_blob_reset(object, NULL, 0);
          reference.Reset();
        }
        memset(out.samples, 0xff, sizeof(out.samples));
        memset(aux.samples, 0xff, sizeof(aux.samples));
        bool enveloped = false;
        bool reference_enveloped = false;
        plaits_blob_render(object, NULL, &parameters,
            out.samples, aux.samples, 24, &enveloped);
        reference.Render(parameters, reference_out, reference_aux, 24,
            &reference_enveloped);
        for (size_t i = 0; i < 24; ++i) {
          if (!std::isfinite(out.samples[i]) ||
              !std::isfinite(aux.samples[i])) {
            printf("ERROR: Bytebeat: non-finite output\n");
            return 1;
          }
        }
        if (memcmp(out.samples, reference_out, sizeof(reference_out)) ||
            memcmp(aux.samples, reference_aux, sizeof(reference_aux)) ||
            enveloped || enveloped != reference_enveloped) {
          printf("ERROR: Bytebeat: wrapper differs from compiled engine\n");
          return 1;
        }
        if (state.before != kCanary || state.after != kCanary ||
            arena.before != kCanary || arena.after != kCanary ||
            out.before != kCanary || out.after != kCanary ||
            aux.before != kCanary || aux.after != kCanary) {
          printf("ERROR: Bytebeat: boundary write\n");
          return 1;
        }
      }
    }
  }
  printf("Bytebeat: wrapper, controls, reset and canaries OK\n");
  return 0;
}
