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
// Beads' controls for the Rings synth.

#include "beads/dsp/rings_synth.h"

namespace beads {

using namespace rings;

// As the Plaits synth's model quantizer.
const float kZoneHysteresis = 0.048f;

void RingsSynth::Init() {
  zone_quantizer_.Init(kNumRingsZones, kZoneHysteresis, false);
  target_.core = RINGS_CORE_PART;
  target_.model = RESONATOR_MODEL_MODAL;
  target_.polyphony = 1;
  target_.fx = FX_ENSEMBLE;
  chord_ = 0;
}

void RingsSynth::ComputeTarget(
    float feedback,
    int polyphony,
    RingsSynthTarget* target) {
  const int zone = zone_quantizer_.Process(feedback);
  if (zone < RESONATOR_MODEL_LAST) {
    target_.core = RINGS_CORE_PART;
    target_.model = zone;
  } else {
    target_.core = RINGS_CORE_STRING_SYNTH;
    target_.fx = zone - RESONATOR_MODEL_LAST;
  }
  target_.polyphony = polyphony == 2 || polyphony == 4 ? polyphony : 1;
  *target = target_;
}

void RingsSynth::ComputeControls(
    float pitch,
    float time,
    float size,
    float shape,
    float dry_wet,
    bool sustain,
    bool trigger,
    bool seed_patched,
    bool pitch_patched,
    RingsSynthControls* controls) {
  Patch* patch = &controls->patch;
  patch->structure = time;
  CONSTRAIN(patch->structure, 0.0f, kRingsMaxStructure);
  patch->brightness = size;
  CONSTRAIN(patch->brightness, 0.0f, 1.0f);
  patch->position = shape;
  CONSTRAIN(patch->position, 0.0f, 1.0f);
  // FREEZE holds the longest decay; strums still play.
  patch->damping = sustain ? 1.0f : dry_wet;
  CONSTRAIN(patch->damping, 0.0f, 1.0f);

  PerformanceState* s = &controls->performance_state;
  s->internal_exciter = true;
  s->internal_note = !pitch_patched;
  // A Beads trigger is Rings' STRUM input; without one, Rings' strummer
  // follows PITCH CV steps while SEED is unpatched.
  s->strum = trigger;
  s->internal_strum = !trigger && !seed_patched;
  s->tonic = kRingsRootNote;
  s->note = pitch;
  CONSTRAIN(s->note, kRingsLowestNote - kRingsRootNote,
            kRingsHighestNote - kRingsRootNote);
  s->fm = 0.0f;

  // Rings' chord hysteresis (cv_scaler.cc), on STRUCTURE.
  const float chord = patch->structure * static_cast<float>(kNumChords - 1);
  const float hysteresis = chord - chord_ > 0.0f ? -0.1f : +0.1f;
  chord_ = static_cast<int32_t>(chord + hysteresis + 0.5f);
  CONSTRAIN(chord_, 0, kNumChords - 1);
  s->chord = chord_;
}

}  // namespace beads
