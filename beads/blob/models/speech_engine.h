// Copyright 2016 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// Various flavours of speech synthesis, as a plain class for a blob: Plaits'
// SpeechEngine (speech_engine.cc), its code unchanged except that the synths'
// methods run in the speech library.

#ifndef BEADS_BLOB_MODELS_SPEECH_ENGINE_H_
#define BEADS_BLOB_MODELS_SPEECH_ENGINE_H_

#include "stmlib/dsp/hysteresis_quantizer.h"
#include "stmlib/utils/buffer_allocator.h"

#include "beads/blob/plaits_blob.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/speech/lpc_speech_synth_controller.h"
#include "plaits/dsp/speech/naive_speech_synth.h"
#include "plaits/dsp/speech/sam_speech_synth.h"

namespace plaits {

class Speech {
 public:
  Speech() { }
  ~Speech() { }

  void Init(
      const beads::PlaitsBlobLibrary* library,
      stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  const beads::PlaitsBlobLibrary* library_;

  stmlib::HysteresisQuantizer2 word_bank_quantizer_;

  NaiveSpeechSynth naive_speech_synth_;
  SAMSpeechSynth sam_speech_synth_;

  LPCSpeechSynthController lpc_speech_synth_controller_;
  LPCSpeechSynthWordBank lpc_speech_synth_word_bank_;

  float* temp_buffer_[2];
  float prosody_amount_;
  float speed_;

  DISALLOW_COPY_AND_ASSIGN(Speech);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_SPEECH_ENGINE_H_
