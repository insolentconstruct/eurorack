// Copyright 2021 Emilie Gillet.
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
// Plaits' ChiptuneEngine's code as a plain class for a blob.

#include "beads/blob/models/chiptune_engine.h"

#include "beads/blob/libraries/chord_bank_library.h"

#include <algorithm>

namespace plaits {

using namespace std;
using namespace stmlib;

void Chiptune::Init(
    const beads::PlaitsBlobLibrary* chord_bank,
    BufferAllocator* allocator) {
  chord_bank_ = chord_bank;
  bass_.Init();
  for (int i = 0; i < kChordNumNotes; ++i) {
    voice_[i].Init();
  }

  plaits_blob_import_chord_bank_init(chord_bank_, &chords_, allocator);

  arpeggiator_.Init();

  arpeggiator_pattern_selector_.Init(12, 0.075f, false);

  envelope_shape_ = 0.0f;
  envelope_state_ = 0.0f;
}

void Chiptune::Reset() {
  plaits_blob_import_chord_bank_reset(chord_bank_, &chords_);
}

void Chiptune::RenderDroneTail(const EngineParameters& parameters,
    float* out, float* aux, size_t size, bool* already_enveloped) {
  EngineParameters drone = parameters;
  drone.trigger = TRIGGER_UNPATCHED;
  Render(drone, out, aux, size, already_enveloped);
}

void Chiptune::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  const float f0 = NoteToFrequency(parameters.note);
  const float shape = parameters.morph * 0.995f;
  const bool clocked = !(parameters.trigger & TRIGGER_UNPATCHED);
  float root_transposition = 1.0f;

  *already_enveloped = clocked;

  if (clocked) {
    if (parameters.trigger & TRIGGER_RISING_EDGE) {
      chords_.set_chord(parameters.harmonics);
      chords_.Sort();

      int pattern = arpeggiator_pattern_selector_.Process(parameters.timbre);
      arpeggiator_.set_mode(ArpeggiatorMode(pattern / 3));
      arpeggiator_.set_range(1 << (pattern % 3));
      arpeggiator_.Clock(chords_.num_notes());
      envelope_state_ = 1.0f;
    }
    const float octave = float(1 << arpeggiator_.octave());
    const float note_f0 = f0 * chords_.sorted_ratio(
        arpeggiator_.note()) * octave;
    root_transposition = octave;
    voice_[0].Render(note_f0, shape, out, size);
  } else {
    float ratios[kChordNumVoices];
    float amplitudes[kChordNumVoices];

    chords_.set_chord(parameters.harmonics);
    plaits_blob_import_chord_bank_compute_chord_inversion(
        chord_bank_, &chords_, parameters.timbre, ratios, amplitudes);
    for (int j = 1; j < kChordNumVoices; j += 2) {
      amplitudes[j] = -amplitudes[j];
    }

    voice_[0].Render(f0 * ratios[0], shape, out, size);
    for (size_t j = 0; j < size; ++j) {
      out[j] = 0.0f + out[j] * amplitudes[0];  // Sum from zero as before.
    }
    for (int voice = 1; voice < kChordNumVoices; ++voice) {
      const float voice_f0 = f0 * ratios[voice];
      voice_[voice].Render(voice_f0, shape, aux, size);
      for (size_t j = 0; j < size; ++j) {
        out[j] += aux[j] * amplitudes[voice];
      }
    }
  }

  // Render bass note.
  bass_.Render(f0 * 0.5f * root_transposition, aux, size);

  // The clocked gain is fixed, as with the TIMBRE attenuverter at noon.
  if (envelope_shape_ != NO_ENVELOPE) {
    const float gain = envelope_state_;
    for (size_t i = 0; i < size; ++i) {
      out[i] *= gain;
    }
  }
}

}  // namespace plaits
