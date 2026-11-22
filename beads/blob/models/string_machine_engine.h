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
// Plaits' StringMachineEngine as a plain class for a blob, its code unchanged
// except its chord bank calls.

#ifndef BEADS_BLOB_MODELS_STRING_MACHINE_ENGINE_H_
#define BEADS_BLOB_MODELS_STRING_MACHINE_ENGINE_H_

#include "stmlib/dsp/filter.h"

#include "beads/blob/plaits_blob.h"
#include "plaits/dsp/chords/chord_bank.h"
#include "plaits/dsp/engine/chord_engine.h"
#include "plaits/dsp/engine2/string_machine_engine.h"
#include "plaits/dsp/fx/ensemble.h"

namespace plaits {

class StringMachine {
 public:
  StringMachine() { }
  ~StringMachine() { }

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

  void ComputeRegistration(float registration, float* amplitudes);

  ChordBank chords_;

  Ensemble ensemble_;
  StringSynthOscillator divide_down_voice_[kChordNumNotes];
  stmlib::NaiveSvf svf_[2];

  float morph_lp_;
  float timbre_lp_;

  DISALLOW_COPY_AND_ASSIGN(StringMachine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_STRING_MACHINE_ENGINE_H_
