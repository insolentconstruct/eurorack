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
// Plaits' speech synths and word banks as a library. Its init decodes the
// five word banks into its RAM, as PlaitsVoice::DecodeSpeechBanks does for
// the compiled-in engines; its exports run Plaits' synth code unchanged.

#include "beads/blob/libraries/speech_library.h"

#include "plaits/dsp/speech/lpc_speech_synth_words.h"

using namespace beads;
using namespace plaits;
using namespace stmlib;

// The context: the word bank table (no absolute pointers in flash) and the
// decoded banks.
struct SpeechLibrary {
  LPCSpeechSynthWordBankData word_banks[LPC_SPEECH_SYNTH_NUM_WORD_BANKS];
  LPCSpeechSynthWordBank* decoded_banks;
};

extern "C" const PlaitsBlobLibraryHeader plaits_blob_header
    __attribute__((section(".blob_header"), used)) = {
  kPlaitsBlobMagic,
  sizeof(PlaitsBlobLibraryHeader),
  kPlaitsBlobAbi2,
  kPlaitsBlobServicesVersion2,
  kPlaitsBlobKindLibrary,
  kSpeechLibrary,
  1,
  0,  // The packer fills the size, init, export table and GOT fields.
  kSpeechLibraryRamSize,
  kSpeechLibraryScratchSize,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  { 0, 0, 0 },
  { 0, 0, 0, 0 }
};

static void SetWordBank(
    LPCSpeechSynthWordBankData* d,
    const uint8_t* data,
    size_t size) {
  d->data = data;
  d->size = size;
}

extern "C" void* plaits_blob_init(
    uint8_t* ram,
    size_t ram_size,
    uint8_t* scratch,
    size_t scratch_size,
    const PlaitsBlobServices* services,
    const PlaitsBlobLibrary* const* libraries) {
  BufferAllocator allocator(ram, ram_size);
  SpeechLibrary* s = allocator.Allocate<SpeechLibrary>();
  SetWordBank(&s->word_banks[0], bank_0, sizeof(bank_0));
  SetWordBank(&s->word_banks[1], bank_1, sizeof(bank_1));
  SetWordBank(&s->word_banks[2], bank_2, sizeof(bank_2));
  SetWordBank(&s->word_banks[3], bank_3, sizeof(bank_3));
  SetWordBank(&s->word_banks[4], bank_4, sizeof(bank_4));
  // Zeroed RAM: the banks decode from the bit stream, not from each other.
  LPCSpeechSynthWordBank* banks = allocator.Allocate<LPCSpeechSynthWordBank>(
      LPC_SPEECH_SYNTH_NUM_WORD_BANKS);
  for (int i = 0; i < LPC_SPEECH_SYNTH_NUM_WORD_BANKS; ++i) {
    BufferAllocator scratch_allocator(scratch, scratch_size);
    banks[i].Init(
        s->word_banks,
        LPC_SPEECH_SYNTH_NUM_WORD_BANKS,
        &scratch_allocator);
    banks[i].Decode(i, &allocator);
  }
  s->decoded_banks = banks;
  return s;
}

void speech_naive_init(
    const PlaitsBlobLibrary* library,
    NaiveSpeechSynth* synth) {
  synth->Init();
}

void speech_naive_render(
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
  synth->Render(
      click, frequency, phoneme, vocal_register, temp, excitation, output,
      size);
}

void speech_sam_init(
    const PlaitsBlobLibrary* library,
    SAMSpeechSynth* synth) {
  synth->Init();
}

void speech_sam_render(
    const PlaitsBlobLibrary* library,
    SAMSpeechSynth* synth,
    bool consonant,
    float frequency,
    float vowel,
    float formant_shift,
    float* excitation,
    float* output,
    size_t size) {
  synth->Render(
      consonant, frequency, vowel, formant_shift, excitation, output, size);
}

void speech_word_bank_init(
    const PlaitsBlobLibrary* library,
    LPCSpeechSynthWordBank* word_bank,
    BufferAllocator* allocator) {
  const SpeechLibrary* s = static_cast<const SpeechLibrary*>(
      library->context);
  word_bank->Init(s->word_banks, LPC_SPEECH_SYNTH_NUM_WORD_BANKS, allocator);
  word_bank->set_decoded_banks(s->decoded_banks);
}

void speech_word_bank_reset(
    const PlaitsBlobLibrary* library,
    LPCSpeechSynthWordBank* word_bank) {
  word_bank->Reset();
}

void speech_controller_init(
    const PlaitsBlobLibrary* library,
    LPCSpeechSynthController* controller,
    LPCSpeechSynthWordBank* word_bank) {
  controller->Init(word_bank);
}

void speech_controller_render(
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
  controller->Render(
      free_running, trigger, bank, frequency, prosody_amount, speed, address,
      formant_shift, gain, excitation, output, size);
}

// In SpeechLibraryExport's order.
extern "C" const void* const plaits_blob_exports[]
    __attribute__((section(".blob_exports"), used)) = {
  reinterpret_cast<const void*>(&speech_naive_init),
  reinterpret_cast<const void*>(&speech_naive_render),
  reinterpret_cast<const void*>(&speech_sam_init),
  reinterpret_cast<const void*>(&speech_sam_render),
  reinterpret_cast<const void*>(&speech_word_bank_init),
  reinterpret_cast<const void*>(&speech_word_bank_reset),
  reinterpret_cast<const void*>(&speech_controller_init),
  reinterpret_cast<const void*>(&speech_controller_render)
};
