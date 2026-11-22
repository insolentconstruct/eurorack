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
// -----------------------------------------------------------------------------
//
// Mutable Instruments' Rings as a synth: Part, StringSynthPart and Strummer,
// driven as rings.cc does. Shared by the app and the Rings synth blob.

#ifndef BEADS_BLOB_SYNTHS_RINGS_SYNTH_CORE_H_
#define BEADS_BLOB_SYNTHS_RINGS_SYNTH_CORE_H_

#include "stmlib/stmlib.h"

#include "beads/blob/synths/rings_synth_controls.h"
#include "rings/dsp/part.h"
#include "rings/dsp/string_synth_part.h"
#include "rings/dsp/strummer.h"

namespace beads {

// Rings' reverb, shared by both cores as in rings.cc.
const size_t kRingsReverbSize = 32768;

class RingsSynthCore {
 public:
  RingsSynthCore() { }
  ~RingsSynthCore() { }

  // In zeroed RAM: Rings' objects start from zeroed .bss.
  void Init();

  // Main loop, codec paused: applies a valid target, then renders one
  // discarded block on the target's core (both cores on entry), so no
  // reconfiguration reaches the audio interrupt. Refuses anything else.
  bool Configure(
      const RingsSynthTarget& target,
      const RingsSynthControls& controls,
      bool entry);

  // Audio interrupt: the strummer, then the target's core, with silent input.
  void Render(
      RingsSynthControls* controls,
      float* out,
      float* aux,
      size_t size);

  static bool Valid(const RingsSynthTarget& target);

  inline const RingsSynthTarget& target() const { return target_; }

 private:
  void Prime(uint8_t core, const RingsSynthControls& controls);

  rings::Part part_;
  rings::StringSynthPart string_synth_;
  rings::Strummer strummer_;
  RingsSynthTarget target_;
  uint16_t reverb_buffer_[kRingsReverbSize];

  DISALLOW_COPY_AND_ASSIGN(RingsSynthCore);
};

STATIC_ASSERT(kNumRingsModels == rings::RESONATOR_MODEL_LAST, rings_models);
STATIC_ASSERT(kNumRingsFx == rings::FX_LAST, rings_fx);
STATIC_ASSERT(kRingsDefaultFx == rings::FX_ENSEMBLE, rings_default_fx);

}  // namespace beads

#endif  // BEADS_BLOB_SYNTHS_RINGS_SYNTH_CORE_H_
