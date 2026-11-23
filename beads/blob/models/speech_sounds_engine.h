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
// The continuously playable sounds from the stock Speech engine, expanded into
// a full engine. HARMONICS traverses naive -> SAM -> LPC phonemes over the whole
// dial; the LPC word banks live in LPCSpeechEngine instead.

#ifndef BEADS_BLOB_MODELS_SPEECH_SOUNDS_ENGINE_H_
#define BEADS_BLOB_MODELS_SPEECH_SOUNDS_ENGINE_H_

#include "beads/blob/plaits_blob.h"
#include "plaits/dsp/engine/engine.h"
#include "beads/blob/models/speech_sounds_controller.h"
#include "plaits/dsp/speech/naive_speech_synth.h"
#include "plaits/dsp/speech/sam_speech_synth.h"

namespace plaits {

class SpeechSounds {
 public:
  SpeechSounds() { }
  ~SpeechSounds() { }

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

  NaiveSpeechSynth naive_speech_synth_;
  SAMSpeechSynth sam_speech_synth_;
  SpeechSoundsController lpc_speech_synth_controller_;
  float* temp_buffer_[2];
  float post_filter_;

  DISALLOW_COPY_AND_ASSIGN(SpeechSounds);
};

#ifndef PLAITS_BLOB
class SpeechSoundsEngine : public Engine {
 public:
  SpeechSoundsEngine() { }
  ~SpeechSoundsEngine() { }

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
  SpeechSounds core_;

  DISALLOW_COPY_AND_ASSIGN(SpeechSoundsEngine);
};
#endif  // PLAITS_BLOB

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_SPEECH_SOUNDS_ENGINE_H_
