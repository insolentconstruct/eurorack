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
// Plaits' ChiptuneEngine as a plain class for a blob, its code unchanged
// except its chord bank calls.

#ifndef BEADS_BLOB_MODELS_CHIPTUNE_ENGINE_H_
#define BEADS_BLOB_MODELS_CHIPTUNE_ENGINE_H_

#include "beads/blob/plaits_blob.h"
#include "plaits/dsp/chords/chord_bank.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/engine2/arpeggiator.h"
#include "plaits/dsp/engine2/chiptune_engine.h"
#include "plaits/dsp/oscillator/nes_triangle_oscillator.h"
#include "plaits/dsp/oscillator/super_square_oscillator.h"

namespace plaits {

class Chiptune {
 public:
  Chiptune() { }
  ~Chiptune() { }

  enum {
    NO_ENVELOPE = 2
  };

  void Init(
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

  SuperSquareOscillator voice_[kChordNumVoices];
  NESTriangleOscillator<> bass_;

  ChordBank chords_;
  Arpeggiator arpeggiator_;
  stmlib::HysteresisQuantizer2 arpeggiator_pattern_selector_;

  float envelope_shape_;
  float envelope_state_;
  float aux_envelope_amount_;

  DISALLOW_COPY_AND_ASSIGN(Chiptune);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_CHIPTUNE_ENGINE_H_
