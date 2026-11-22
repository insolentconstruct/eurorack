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
// Plaits' speech synths and word banks as a library: its ID, RAM, exports
// and their signatures. The library decodes the five word banks once at synth
// entry; a dependent keeps its synths in its own state and runs the library's
// code on them through the plaits_blob_import_speech_* calls
// (speech_imports.cc).

#ifndef BEADS_BLOB_LIBRARIES_SPEECH_LIBRARY_H_
#define BEADS_BLOB_LIBRARIES_SPEECH_LIBRARY_H_

#include "stmlib/stmlib.h"
#include "stmlib/utils/buffer_allocator.h"

#include "beads/blob/plaits_blob.h"
#include "plaits/dsp/speech/lpc_speech_synth_controller.h"
#include "plaits/dsp/speech/naive_speech_synth.h"
#include "plaits/dsp/speech/sam_speech_synth.h"

namespace beads {

// Permanent ID (profile-features.json).
const uint32_t kSpeechLibrary = 3;
// The decoded word banks, as kPlaitsSpeechRamSize; scratch holds one bank
// while it is decoded.
const size_t kSpeechLibraryRamSize = 36 * 1024;
const size_t kSpeechLibraryScratchSize =
    plaits::kLPCSpeechSynthMaxFrames * sizeof(plaits::LPCSpeechSynth::Frame) +
    plaits::kLPCSpeechSynthMaxWords * sizeof(int);

enum SpeechLibraryExport {
  SPEECH_NAIVE_INIT,
  SPEECH_NAIVE_RENDER,
  SPEECH_SAM_INIT,
  SPEECH_SAM_RENDER,
  SPEECH_WORD_BANK_INIT,
  SPEECH_WORD_BANK_RESET,
  SPEECH_CONTROLLER_INIT,
  SPEECH_CONTROLLER_RENDER,
  SPEECH_LIBRARY_EXPORT_LAST
};

// Each export takes the library first, then the synth it runs on and the
// synth method's own arguments.
typedef void SpeechNaiveInit(
    const PlaitsBlobLibrary* library,
    plaits::NaiveSpeechSynth* synth);
typedef void SpeechNaiveRender(
    const PlaitsBlobLibrary* library,
    plaits::NaiveSpeechSynth* synth,
    bool click,
    float frequency,
    float phoneme,
    float vocal_register,
    float* temp,
    float* excitation,
    float* output,
    size_t size);
typedef void SpeechSamInit(
    const PlaitsBlobLibrary* library,
    plaits::SAMSpeechSynth* synth);
typedef void SpeechSamRender(
    const PlaitsBlobLibrary* library,
    plaits::SAMSpeechSynth* synth,
    bool consonant,
    float frequency,
    float vowel,
    float formant_shift,
    float* excitation,
    float* output,
    size_t size);
// A dependent's word bank copies frames from the library's decoded banks.
typedef void SpeechWordBankInit(
    const PlaitsBlobLibrary* library,
    plaits::LPCSpeechSynthWordBank* word_bank,
    stmlib::BufferAllocator* allocator);
typedef void SpeechWordBankReset(
    const PlaitsBlobLibrary* library,
    plaits::LPCSpeechSynthWordBank* word_bank);
typedef void SpeechControllerInit(
    const PlaitsBlobLibrary* library,
    plaits::LPCSpeechSynthController* controller,
    plaits::LPCSpeechSynthWordBank* word_bank);
typedef void SpeechControllerRender(
    const PlaitsBlobLibrary* library,
    plaits::LPCSpeechSynthController* controller,
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
    size_t size);

}  // namespace beads

extern "C" {

// The library's exports.
beads::SpeechNaiveInit speech_naive_init;
beads::SpeechNaiveRender speech_naive_render;
beads::SpeechSamInit speech_sam_init;
beads::SpeechSamRender speech_sam_render;
beads::SpeechWordBankInit speech_word_bank_init;
beads::SpeechWordBankReset speech_word_bank_reset;
beads::SpeechControllerInit speech_controller_init;
beads::SpeechControllerRender speech_controller_render;

// A dependent's calls to them.
beads::SpeechNaiveInit plaits_blob_import_speech_naive_init;
beads::SpeechNaiveRender plaits_blob_import_speech_naive_render;
beads::SpeechSamInit plaits_blob_import_speech_sam_init;
beads::SpeechSamRender plaits_blob_import_speech_sam_render;
beads::SpeechWordBankInit plaits_blob_import_speech_word_bank_init;
beads::SpeechWordBankReset plaits_blob_import_speech_word_bank_reset;
beads::SpeechControllerInit plaits_blob_import_speech_controller_init;
beads::SpeechControllerRender plaits_blob_import_speech_controller_render;

}  // extern "C"

#endif  // BEADS_BLOB_LIBRARIES_SPEECH_LIBRARY_H_
