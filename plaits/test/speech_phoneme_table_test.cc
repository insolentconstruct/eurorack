// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct
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
// Speech: HARMONICS from a word bank down to the phonemes, mid-word and at the
// end of a word. Without the fix, the first case fails; the second reads one
// frame out of range, which this check cannot see.
// "./speech_phoneme_table_test hash" prints a hash of the output on other
// paths, to compare builds without sanitizers.
//
// g++ -DTEST -O1 -g -I. -fsanitize=address,undefined \
//   -fno-sanitize-recover=all -no-pie \
//   plaits/test/speech_phoneme_table_test.cc \
//   plaits/dsp/speech/lpc_speech_synth.cc \
//   plaits/dsp/speech/lpc_speech_synth_controller.cc \
//   plaits/dsp/speech/lpc_speech_synth_phonemes.cc \
//   plaits/dsp/speech/lpc_speech_synth_words.cc \
//   plaits/resources.cc stmlib/dsp/units.cc stmlib/utils/random.cc \
//   -o speech_phoneme_table_test && ./speech_phoneme_table_test

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "stmlib/utils/buffer_allocator.h"

#define private public
#include "plaits/dsp/speech/lpc_speech_synth_controller.h"
#undef private
#include "plaits/dsp/speech/lpc_speech_synth_words.h"

using namespace plaits;

void Check(bool condition, const char* message) {
  if (!condition) {
    printf("FAIL: %s\n", message);
    exit(1);
  }
}

char ram[16384];
LPCSpeechSynthWordBank word_bank;
LPCSpeechSynthController speech;

uint32_t hash = 2166136261u;

void Render(int bank, bool trigger, float address = 0.99f,
    bool free_running = false) {
  float excitation[kBlockSize];
  float out[kBlockSize];
  speech.Render(free_running, trigger, bank, 100.0f / 48000.0f, 0.3f, 0.5f,
      address, 0.5f, 0.8f, excitation, out, kBlockSize);
  for (size_t i = 0; i < kBlockSize; ++i) {
    uint32_t bits[2];
    memcpy(&bits[0], &excitation[i], sizeof(float));
    memcpy(&bits[1], &out[i], sizeof(float));
    hash = ((hash ^ bits[0]) * 16777619 ^ bits[1]) * 16777619;
  }
}

// Paths that never step a word's frame on the phonemes, to compare builds.
void PrintHash() {
  const float addresses[] = { 0.0f, 0.3f, 0.6f, 0.99f };
  for (int free_running = 0; free_running < 2; ++free_running) {
    for (int bank = -1; bank < LPC_SPEECH_SYNTH_NUM_WORD_BANKS; ++bank) {
      for (int a = 0; a < 4; ++a) {
        for (int i = 0; i < 3000; ++i) {
          Render(bank, i % 1000 == 0, addresses[a], free_running);
        }
      }
    }
  }
  // Word bank to word bank mid-word, then phonemes to a word bank.
  for (int i = 0; i < 2000; ++i) {
    Render(i < 100 ? 0 : 3, i == 0);
  }
  for (int i = 0; i < 2000; ++i) {
    Render(i < 1000 ? -1 : 2, i == 0 || i == 1000, 0.6f);
  }
  // A dip to the phonemes that ends within a word's frame.
  for (int i = 0; i < 100; ++i) {
    Render(4, i == 0);
  }
  while (speech.remaining_frame_samples_ < 2 * kBlockSize) {
    Render(4, false);
  }
  Render(-1, false);
  for (int i = 0; i < 1000; ++i) {
    Render(4, false);
  }
  printf("hash %08x\n", hash);
}

int main(int argc, char** argv) {
  stmlib::BufferAllocator allocator(ram, sizeof(ram));
  word_bank.Init(word_banks_, LPC_SPEECH_SYNTH_NUM_WORD_BANKS, &allocator);
  speech.Init(&word_bank);
  if (argc > 1 && !strcmp(argv[1], "hash")) {
    PrintHash();
    return 0;
  }
  // Pass "ended" to run only the second case.
  const bool ended_only = argc > 1 && !strcmp(argv[1], "ended");

  // The last word of the last bank, mid-word and held at its end.
  const int bank = LPC_SPEECH_SYNTH_NUM_WORD_BANKS - 1;
  for (int ended = ended_only; ended < 2; ++ended) {
    for (int i = 0; i < (ended ? 4000 : 100); ++i) {
      Render(bank, i == 0);
    }
    Check(speech.playback_frame_ >= kLPCSpeechSynthNumPhonemes,
        "the word plays frames past the phoneme table");

    // HARMONICS to the phonemes: playback must stay within their 15 frames.
    for (int i = 0; i < 4000; ++i) {
      Render(-1, false);
      if (speech.playback_frame_ >= kLPCSpeechSynthNumPhonemes) {
        printf("phoneme frame %d\n", speech.playback_frame_);
      }
      Check(speech.playback_frame_ < kLPCSpeechSynthNumPhonemes,
          "a word's frame number indexes the phoneme table");
    }
    printf("PASS %s\n", ended ? "after the word" : "mid-word");
  }
  return 0;
}
