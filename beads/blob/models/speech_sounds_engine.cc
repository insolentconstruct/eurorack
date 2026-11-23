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

#include "beads/blob/models/speech_sounds_engine.h"

#include "beads/blob/libraries/speech_library.h"

namespace plaits {

using namespace stmlib;

void SpeechSounds::Init(
    const beads::PlaitsBlobLibrary* library,
    BufferAllocator* allocator) {
  library_ = library;
  plaits_blob_import_speech_naive_init(library_, &naive_speech_synth_);
  plaits_blob_import_speech_sam_init(library_, &sam_speech_synth_);
  // Phoneme scans do not need a word bank.
  lpc_speech_synth_controller_.Init();
  temp_buffer_[0] = allocator->Allocate<float>(kMaxBlockSize);
  temp_buffer_[1] = allocator->Allocate<float>(kMaxBlockSize);
  post_filter_ = 0.0f;
}

void SpeechSounds::Reset() {
  post_filter_ = 0.0f;
}

void SpeechSounds::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  const float f0 = NoteToFrequency(parameters.note);
  const bool trigger = parameters.trigger & TRIGGER_RISING_EDGE;

  // Traverse naive, SAM and LPC phonemes across HARMONICS.
  const float group = parameters.harmonics * 2.0f;
  float blend;
  if (group <= 1.0f) {
    plaits_blob_import_speech_naive_render(
        library_,
        &naive_speech_synth_,
        trigger,
        f0,
        parameters.morph,
        parameters.timbre,
        temp_buffer_[0],
        aux,
        out,
        size);
    blend = group;
  } else {
    lpc_speech_synth_controller_.Render(
        parameters.trigger & TRIGGER_UNPATCHED,
        trigger,
        f0,
        0.0f,
        0.0f,
        parameters.morph,
        parameters.timbre,
        1.0f,
        aux,
        out,
        size);
    blend = 2.0f - group;
  }

  plaits_blob_import_speech_sam_render(
      library_,
      &sam_speech_synth_,
      trigger,
      f0,
      parameters.morph,
      parameters.timbre,
      temp_buffer_[0],
      temp_buffer_[1],
      size);

  blend *= blend * (3.0f - 2.0f * blend);
  blend *= blend * (3.0f - 2.0f * blend);
  for (size_t i = 0; i < size; ++i) {
    aux[i] += (temp_buffer_[0][i] - aux[i]) * blend;
    out[i] += (temp_buffer_[1][i] - out[i]) * blend;
  }

  // Palette's fourth macro stays at noon.
  for (size_t i = 0; i < size; ++i) {
    const float voice = out[i];
    ONE_POLE(post_filter_, voice, 0.12f);
    out[i] = voice + (voice - post_filter_) * 0.0f;
  }
}

}  // namespace plaits
