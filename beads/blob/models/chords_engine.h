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
// -----------------------------------------------------------------------------
//
// Chords: wavetable and divide-down organ/string machine, as a plain class for
// a blob: Plaits' ChordEngine (chord_engine.cc), its code unchanged except that
// the chord bank's methods run in the chord bank library and its waves come
// from the Plaits wavetables library, without the virtual functions a blob
// cannot construct.

#ifndef BEADS_BLOB_MODELS_CHORDS_ENGINE_H_
#define BEADS_BLOB_MODELS_CHORDS_ENGINE_H_

#include "beads/blob/plaits_blob.h"
#include "plaits/dsp/chords/chord_bank.h"
#include "plaits/dsp/engine/chord_engine.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/oscillator/string_synth_oscillator.h"
#include "plaits/dsp/oscillator/wavetable_oscillator.h"

namespace plaits {

// The waves Plaits' ChordEngine plays from its wavetable table.
const int kChordNumWaves = 15;

class Chords {
 public:
  Chords() { }
  ~Chords() { }

  // waves is the library's copy of wav_integrated_waves.
  void Init(
      const int16_t* waves,
      const beads::PlaitsBlobLibrary* chord_bank,
      stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  const beads::PlaitsBlobLibrary* chord_bank_;
  // Plaits' wavetable table, built in the object: a blob holds no absolute
  // pointers.
  const int16_t* wavetable_[kChordNumWaves];

  void ComputeRegistration(float registration, float* amplitudes);
  int ComputeChordInversion(
      float inversion,
      float* ratios,
      float* amplitudes);

  StringSynthOscillator divide_down_voice_[kChordNumVoices];
  WavetableOscillator<128, 15> wavetable_voice_[kChordNumVoices];
  ChordBank chords_;

  float morph_lp_;
  float timbre_lp_;

  DISALLOW_COPY_AND_ASSIGN(Chords);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_CHORDS_ENGINE_H_
