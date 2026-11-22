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
//
// -----------------------------------------------------------------------------
//
// A dependent's calls into the speech library. Each loads one export from the
// library record and tail-calls it, the shape the gate accepts; their own
// translation unit keeps them out of line.

#include "beads/blob/libraries/speech_library.h"

using namespace beads;
using namespace plaits;
using namespace stmlib;

template<typename T>
inline T* Export(const PlaitsBlobLibrary* library, SpeechLibraryExport e) {
  return reinterpret_cast<T*>(library->exports[e]);
}

void plaits_blob_import_speech_naive_init(
    const PlaitsBlobLibrary* library,
    NaiveSpeechSynth* synth) {
  Export<SpeechNaiveInit>(library, SPEECH_NAIVE_INIT)(library, synth);
}

void plaits_blob_import_speech_naive_render(
    const PlaitsBlobLibrary* library,
    NaiveSpeechSynth* synth,
    bool click,
    float frequency,
    float phoneme,
    float vocal_register,
    float* temp,
    float* excitation,
    float* output,
    size_t size) {
  Export<SpeechNaiveRender>(library, SPEECH_NAIVE_RENDER)(
      library, synth, click, frequency, phoneme, vocal_register, temp,
      excitation, output, size);
}

void plaits_blob_import_speech_sam_init(
    const PlaitsBlobLibrary* library,
    SAMSpeechSynth* synth) {
  Export<SpeechSamInit>(library, SPEECH_SAM_INIT)(library, synth);
}

void plaits_blob_import_speech_sam_render(
    const PlaitsBlobLibrary* library,
    SAMSpeechSynth* synth,
    bool consonant,
    float frequency,
    float vowel,
    float formant_shift,
    float* excitation,
    float* output,
    size_t size) {
  Export<SpeechSamRender>(library, SPEECH_SAM_RENDER)(
      library, synth, consonant, frequency, vowel, formant_shift, excitation,
      output, size);
}

void plaits_blob_import_speech_word_bank_init(
    const PlaitsBlobLibrary* library,
    LPCSpeechSynthWordBank* word_bank,
    BufferAllocator* allocator) {
  Export<SpeechWordBankInit>(library, SPEECH_WORD_BANK_INIT)(
      library, word_bank, allocator);
}

void plaits_blob_import_speech_word_bank_reset(
    const PlaitsBlobLibrary* library,
    LPCSpeechSynthWordBank* word_bank) {
  Export<SpeechWordBankReset>(library, SPEECH_WORD_BANK_RESET)(
      library, word_bank);
}

void plaits_blob_import_speech_controller_init(
    const PlaitsBlobLibrary* library,
    LPCSpeechSynthController* controller,
    LPCSpeechSynthWordBank* word_bank) {
  Export<SpeechControllerInit>(library, SPEECH_CONTROLLER_INIT)(
      library, controller, word_bank);
}

void plaits_blob_import_speech_controller_render(
    const PlaitsBlobLibrary* library,
    LPCSpeechSynthController* controller,
    bool free_running,
    bool trigger,
    int bank,
    float frequency,
    float prosody_amount,
    float speed,
    float address,
    float formant_shift,
    float gain,
    float* excitation,
    float* output,
    size_t size) {
  Export<SpeechControllerRender>(library, SPEECH_CONTROLLER_RENDER)(
      library, controller, free_running, trigger, bank, frequency,
      prosody_amount, speed, address, formant_shift, gain, excitation, output,
      size);
}
