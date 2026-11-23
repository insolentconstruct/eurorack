// Copyright 2016 Emilie Gillet.
// Copyright 2026 Rubato Audio.
// Copyright 2026 Insolent Construct.
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
// -----------------------------------------------------------------------------
//
// The LPC word banks from the stock Speech engine and recipe resources, with
// the stock word and vocal-tract controls preserved and playback speed plus
// recorded prosody available directly.

#ifndef BEADS_BLOB_MODELS_LPC_WORDS_ENGINE_H_
#define BEADS_BLOB_MODELS_LPC_WORDS_ENGINE_H_

#include "stmlib/dsp/hysteresis_quantizer.h"

#include "plaits/dsp/engine/engine.h"
#include "beads/blob/models/lpc_words_controller.h"
#include "beads/blob/plaits_blob.h"

namespace plaits {

class LPCWords {
 public:
  LPCWords() { }
  ~LPCWords() { }

  void Init(
      const beads::PlaitsBlobLibrary* library,
      stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);
  inline void set_prosody_amount(float prosody_amount) {
    prosody_amount_ = prosody_amount;
  }

 private:
  const beads::PlaitsBlobLibrary* library_;

  stmlib::HysteresisQuantizer2 word_bank_quantizer_;
  LPCWordsController lpc_speech_synth_controller_;
  LPCSpeechSynthWordBank lpc_speech_synth_word_bank_;
  float prosody_amount_;

  DISALLOW_COPY_AND_ASSIGN(LPCWords);
};

#ifndef PLAITS_BLOB
class LPCWordsEngine : public Engine {
 public:
  LPCWordsEngine() { }
  ~LPCWordsEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    core_.Init(NULL, allocator);
  }

  virtual void Reset() {
    core_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    core_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  LPCWords core_;

  DISALLOW_COPY_AND_ASSIGN(LPCWordsEngine);
};
#endif  // PLAITS_BLOB

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_LPC_WORDS_ENGINE_H_
