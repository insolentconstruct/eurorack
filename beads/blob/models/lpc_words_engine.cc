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

#include "beads/blob/models/lpc_words_engine.h"

#include "beads/blob/libraries/speech_library.h"
#include "plaits/dsp/speech/lpc_speech_synth_words.h"

namespace plaits {

using namespace stmlib;

void LPCWords::Init(
    const beads::PlaitsBlobLibrary* library,
    BufferAllocator* allocator) {
  library_ = library;
  plaits_blob_import_speech_word_bank_init(
      library_, &lpc_speech_synth_word_bank_, allocator);
  lpc_speech_synth_controller_.Init(&lpc_speech_synth_word_bank_);
  word_bank_quantizer_.Init(LPC_SPEECH_SYNTH_NUM_WORD_BANKS, 0.1f, false);
  prosody_amount_ = 0.0f;
}

void LPCWords::Reset() {
  plaits_blob_import_speech_word_bank_reset(
      library_, &lpc_speech_synth_word_bank_);
}

void LPCWords::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  const int word_bank = word_bank_quantizer_.Process(parameters.harmonics);
  const bool free_running = parameters.trigger & TRIGGER_UNPATCHED;
  const bool trigger = parameters.trigger & TRIGGER_RISING_EDGE;
  const bool replay_prosody = word_bank >= 0 && !free_running;
  *already_enveloped = replay_prosody;

  // Palette's fourth macro stays at noon.
  const float speed = (0.5f - 0.5f) * 2.0f;
  lpc_speech_synth_controller_.Render(
      free_running,
      trigger,
      word_bank,
      NoteToFrequency(parameters.note),
      prosody_amount_,
      speed,
      parameters.morph,
      parameters.timbre,
      replay_prosody ? parameters.accent : 1.0f,
      aux,
      out,
      size);
}

}  // namespace plaits
