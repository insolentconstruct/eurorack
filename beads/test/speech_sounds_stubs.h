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
// Direct speech imports for compiled-in reference engines in the emulator.

#ifndef BEADS_TEST_SPEECH_SOUNDS_STUBS_H_
#define BEADS_TEST_SPEECH_SOUNDS_STUBS_H_

#include "beads/blob/libraries/speech_library.h"
#include "plaits/dsp/speech/lpc_speech_synth_words.h"

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

extern "C" void plaits_blob_import_speech_word_bank_init(
    const beads::PlaitsBlobLibrary* library,
    plaits::LPCSpeechSynthWordBank* bank,
    stmlib::BufferAllocator* allocator) {
  bank->Init(plaits::word_banks_, LPC_SPEECH_SYNTH_NUM_WORD_BANKS, allocator);
}

extern "C" void plaits_blob_import_speech_word_bank_reset(
    const beads::PlaitsBlobLibrary* library,
    plaits::LPCSpeechSynthWordBank* bank) {
  bank->Reset();
}

#endif  // BEADS_TEST_SPEECH_SOUNDS_STUBS_H_
