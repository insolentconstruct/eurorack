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
// Beads' controls for the Rings synth: FEEDBACK's twelve zones, the knobs as
// Rings' parameters, and Beads' triggers as strums.

#ifndef BEADS_DSP_RINGS_SYNTH_H_
#define BEADS_DSP_RINGS_SYNTH_H_

#include "stmlib/stmlib.h"
#include "stmlib/dsp/hysteresis_quantizer.h"

#include "beads/blob/synths/rings_synth_core.h"

namespace beads {

// Six resonator models, then Disastrous peace with each of its six FX.
const int kNumRingsZones = 12;
// C3, as both Beads synths. Rings tunes for 48 kHz: Beads plays 0.33 cents
// flat, left as is so the core stays vanilla.
const float kRingsRootNote = 48.0f;
// Rings' own reach: TUNE 12 to 72 plus V/OCT at its factory calibration.
const float kRingsLowestNote = 12.0f + 66.67f - 84.26f;
const float kRingsHighestNote = 72.0f + 66.67f;
// Rings' STRUCTURE range (cv_scaler.cc).
const float kRingsMaxStructure = 0.9995f;

class RingsSynth {
 public:
  RingsSynth() { }
  ~RingsSynth() { }

  void Init();

  // FEEDBACK's zone and the saved polyphony, as the core's target. The core
  // that does not play keeps its last model or FX.
  void ComputeTarget(float feedback, int polyphony, RingsSynthTarget* target);

  // One block's controls. pitch: semitones from C3 after Pitch scale.
  // trigger: SEED, its gate or the DENSITY clock fired in this block.
  void ComputeControls(
      float pitch,
      float time,
      float size,
      float shape,
      float dry_wet,
      bool sustain,
      bool trigger,
      bool seed_patched,
      bool pitch_patched,
      RingsSynthControls* controls);

 private:
  stmlib::HysteresisQuantizer2 zone_quantizer_;
  RingsSynthTarget target_;
  int32_t chord_;

  DISALLOW_COPY_AND_ASSIGN(RingsSynth);
};

}  // namespace beads

#endif  // BEADS_DSP_RINGS_SYNTH_H_
