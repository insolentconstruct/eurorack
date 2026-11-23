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
// LPC Words' decoded-bank copying, trigger modes and control extremes.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "beads/blob/models/lpc_words_engine.h"
#include "beads/blob/libraries/speech_library.h"
#include "plaits/dsp/speech/lpc_speech_synth_words.h"
#include "stmlib/utils/random.h"

static plaits::LPCSpeechSynthWordBank decoded_banks[5];

extern "C" void plaits_blob_import_speech_word_bank_init(
    const beads::PlaitsBlobLibrary* library,
    plaits::LPCSpeechSynthWordBank* bank,
    stmlib::BufferAllocator* allocator) {
  bank->Init(plaits::word_banks_, LPC_SPEECH_SYNTH_NUM_WORD_BANKS, allocator);
  bank->set_decoded_banks(decoded_banks);
}

extern "C" void plaits_blob_import_speech_word_bank_reset(
    const beads::PlaitsBlobLibrary* library,
    plaits::LPCSpeechSynthWordBank* bank) {
  bank->Reset();
}

int main(int argc, char** argv) {
  float library_ram[36 * 1024 / sizeof(float)];
  float scratch[beads::kSpeechLibraryScratchSize / sizeof(float)];
  stmlib::BufferAllocator library_allocator(library_ram, sizeof(library_ram));
  for (int i = 0; i < LPC_SPEECH_SYNTH_NUM_WORD_BANKS; ++i) {
    stmlib::BufferAllocator scratch_allocator(scratch, sizeof(scratch));
    decoded_banks[i].Init(plaits::word_banks_,
        LPC_SPEECH_SYNTH_NUM_WORD_BANKS, &scratch_allocator);
    decoded_banks[i].Decode(i, &library_allocator);
  }
  plaits::LPCWords engine;
  float arena[beads::kSpeechLibraryScratchSize / sizeof(float)];
  stmlib::BufferAllocator allocator(arena, sizeof(arena));
  engine.Init(NULL, &allocator);
  plaits::EngineParameters parameters;
  memset(&parameters, 0, sizeof(parameters));
  parameters.accent = 0.75f;
  float out[24];
  float aux[24];
  bool already_enveloped = false;
  FILE* render = argc > 1 ? fopen(argv[1], "wb") : NULL;
  if (argc > 1 && !render) {
    return 1;
  }
  stmlib::Random::Seed(0x21);
  for (int note = -24; note <= 156; note += 12) {
    parameters.note = note;
    for (int control = 0; control < 20; ++control) {
      parameters.harmonics = (control % 5) * 0.25f;
      parameters.timbre = (control / 5) & 1 ? 1.0f : 0.0f;
      parameters.morph = (control / 10) & 1 ? 1.0f : 0.0f;
      for (int block = 0; block < 128; ++block) {
        parameters.trigger = block < 64 ? plaits::TRIGGER_UNPATCHED
            : block % 17 == 0 ? plaits::TRIGGER_RISING_EDGE : 0;
        memset(out, 0xff, sizeof(out));
        memset(aux, 0xff, sizeof(aux));
        engine.Render(parameters, out, aux, 24, &already_enveloped);
        for (size_t i = 0; i < 24; ++i) {
          if (!std::isfinite(out[i]) || !std::isfinite(aux[i])) {
            printf("ERROR: LPC Words: non-finite output\n");
            return 1;
          }
        }
        if (already_enveloped != !(parameters.trigger & plaits::TRIGGER_UNPATCHED)) {
          printf("ERROR: LPC Words: envelope flag\n");
          return 1;
        }
        if (render) {
          fwrite(out, sizeof(out), 1, render);
          fwrite(aux, sizeof(aux), 1, render);
        }
        if (block == 63) {
          engine.Reset();
        }
      }
    }
  }
  if (render && fclose(render)) {
    return 1;
  }
  printf("LPC Words: banks, triggers, extremes and warm reset OK\n");
  return 0;
}
