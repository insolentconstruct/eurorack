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
// Speech Sounds' three synth groups, trigger modes and poisoned outputs.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "beads/blob/models/speech_sounds_engine.h"
#include "beads/blob/libraries/speech_library.h"
#include "stmlib/utils/random.h"

extern "C" void plaits_blob_import_speech_naive_init(
    const beads::PlaitsBlobLibrary* library,
    plaits::NaiveSpeechSynth* synth) {
  synth->Init();
}

extern "C" void plaits_blob_import_speech_naive_render(
    const beads::PlaitsBlobLibrary* library,
    plaits::NaiveSpeechSynth* synth,
    bool click, float frequency, float phoneme, float vocal_register,
    float* temp, float* excitation, float* output, size_t size) {
  synth->Render(click, frequency, phoneme, vocal_register,
      temp, excitation, output, size);
}

extern "C" void plaits_blob_import_speech_sam_init(
    const beads::PlaitsBlobLibrary* library,
    plaits::SAMSpeechSynth* synth) {
  synth->Init();
}

extern "C" void plaits_blob_import_speech_sam_render(
    const beads::PlaitsBlobLibrary* library,
    plaits::SAMSpeechSynth* synth,
    bool consonant, float frequency, float vowel, float formant_shift,
    float* excitation, float* output, size_t size) {
  synth->Render(consonant, frequency, vowel, formant_shift,
      excitation, output, size);
}

int main(int argc, char** argv) {
  plaits::SpeechSounds engine;
  float arena[48];
  stmlib::BufferAllocator allocator(arena, sizeof(arena));
  engine.Init(NULL, &allocator);
  plaits::EngineParameters parameters;
  memset(&parameters, 0, sizeof(parameters));
  float out[24];
  float aux[24];
  bool already_enveloped = true;
  FILE* render = argc > 1 ? fopen(argv[1], "wb") : NULL;
  if (argc > 1 && !render) {
    return 1;
  }
  stmlib::Random::Seed(0x21);
  for (int note = -24; note <= 156; note += 12) {
    parameters.note = note;
    for (int control = 0; control < 12; ++control) {
      parameters.harmonics = (control % 3) * 0.5f;
      parameters.timbre = (control / 3) & 1 ? 1.0f : 0.0f;
      parameters.morph = (control / 6) & 1 ? 1.0f : 0.0f;
      for (int block = 0; block < 64; ++block) {
        parameters.trigger = block < 32 ? plaits::TRIGGER_UNPATCHED
            : block % 7 == 0 ? plaits::TRIGGER_RISING_EDGE : 0;
        memset(out, 0xff, sizeof(out));
        memset(aux, 0xff, sizeof(aux));
        engine.Render(parameters, out, aux, 24, &already_enveloped);
        for (size_t i = 0; i < 24; ++i) {
          if (!std::isfinite(out[i]) || !std::isfinite(aux[i])) {
            printf("ERROR: Speech Sounds: non-finite output\n");
            return 1;
          }
        }
        if (already_enveloped) {
          printf("ERROR: Speech Sounds: envelope flag\n");
          return 1;
        }
        if (render) {
          fwrite(out, sizeof(out), 1, render);
          fwrite(aux, sizeof(aux), 1, render);
        }
        if (block == 31) {
          engine.Reset();
        }
      }
    }
  }
  if (render && fclose(render)) {
    return 1;
  }
  printf("Speech Sounds: groups, triggers, extremes and warm reset OK\n");
  return 0;
}
