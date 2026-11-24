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

#include <algorithm>
#include <cstring>

#include "beads/blob/plaits_blob.h"
#include "beads/dsp/pitch_quantizer.h"

#include "beads/dsp/fx/reverb_decay_render.h"

namespace beads {

using namespace rings;
using namespace std;

// As the Plaits synth's model quantizer.
const float kZoneHysteresis = 0.048f;

#ifndef BEADS_FIXED_REVERB_DECAY
void RingsSynth::Init() {
  zone_quantizer_.Init(kNumRingsZones, kZoneHysteresis, false);
  target_.core = RINGS_CORE_PART;
  target_.model = 0;
  target_.polyphony = 1;
  target_.fx = kRingsDefaultFx;
  chord_ = 0;
}

void RingsSynth::Init(uint16_t* reverb_buffer) {
  Init();
  clouds_style_ = false;
  output_limiter_mode_ = OUTPUT_LIMITER_STOCK;
  const float sr = kSynthSampleRate;
  reverb_.Init(reverb_buffer, sr);
  trigger_generator_.Init(sr);
  pre_reverb_limiter_.Init(1.0f / (sr * 4.0f), 0.9f);
  output_limiter_.Init(1.0f / (sr * 6.0f), 0.9f);
  stereo_width_ = 0.0f;
  width_enabled_ = false;
  pitch_scale_ = PITCH_SCALE_OFF;
  polyphony_ = 1;
  feedback_ = 0.0f;
  ComputeControls(0.0f, 0.5f, 0.5f, 0.5f, 0.5f, false, false, false, false,
                  &controls_);
  set_blob(NULL);
}

/* static */
bool RingsSynth::Fits(const uint8_t* blob, size_t ram_size) {
  if (!blob) {
    return false;
  }
  PlaitsBlobSynthHeader h;
  memcpy(&h, blob, sizeof(h));
  return h.kind == kPlaitsBlobKindSynth && h.id == kRingsSynthId &&
      h.ram_size <= ram_size && h.controls_size == sizeof(RingsSynthControls) &&
      h.block_size == kRingsBlockSize;
}

void RingsSynth::set_blob(const uint8_t* blob) {
  if (blob) {
    PlaitsBlobSynthHeader h;
    memcpy(&h, blob, sizeof(h));
    set_entries(
        reinterpret_cast<RingsSynthInitFn>(blob + h.init_offset),
        reinterpret_cast<RingsSynthConfigureFn>(blob + h.configure_offset),
        reinterpret_cast<RingsSynthRenderFn>(blob + h.render_offset));
  } else {
    set_entries(NULL, NULL, NULL);
  }
}

void RingsSynth::set_entries(
    RingsSynthInitFn init,
    RingsSynthConfigureFn configure,
    RingsSynthRenderFn render) {
  init_ = init;
  configure_ = configure;
  render_ = render;
  context_ = NULL;
  pending_ = false;
}

void RingsSynth::InitVoices(
    uint8_t* ram,
    size_t ram_size,
    const PlaitsBlobServices* services,
    float feedback) {
  context_ = NULL;
  pending_ = false;
  if (!init_) {
    return;
  }
  // Rings' objects start from zeroed .bss; a used bank would leave old bytes.
  fill(&ram[0], &ram[ram_size], 0);
  context_ = init_(ram, ram_size, services);
  // FEEDBACK's zone now; the controls Rings last played with prime it.
  feedback_ = feedback;
  ComputeTarget(feedback_, polyphony_, &configured_);
  wanted_ = configured_;
  settle_ = kRingsSettleBlocks;
  configure_(context_, &configured_, &controls_, true);
}

void RingsSynth::Configure() {
  if (pending_ && context_) {
    // The last block's controls: the configure block is primed with them.
    if (configure_(context_, &wanted_, &controls_, false)) {
      configured_ = wanted_;
    }
  }
  pending_ = false;
}
#endif  // BEADS_FIXED_REVERB_DECAY

void RingsSynth::BEADS_DECAY_RENDER(Render)(
    SeedButtonState seed_button_state,
    const stmlib::GateFlags* seed_input,
    const bool* freeze,
    Synth::Resolution resolution,
    const ModulatedParameter& density,
    const Parameters& parameters,
    const Processor::MixParameters& mix_parameters,
    FloatFrame* out,
    size_t size) {
  Parameters p = parameters;
  const bool pitch_patched = p.pitch.has_cv;
  float pitch = p.pitch.base + (pitch_patched ? p.pitch.cv : 0.0f);
  if (pitch_scale_ != PITCH_SCALE_OFF) {
    pitch = QuantizePitch(pitch, pitch_scale_);
  }

  float expo_fm_amount = 20.0f * (fabsf(density.base - 0.5f) - 0.225f);
  CONSTRAIN(expo_fm_amount, 0.0f, 1.0f);
  TriggerGenerator::Output triggers[size];
  trigger_generator_.Render(
      seed_button_state,
      seed_input,
      density.base,
      (1.0f - expo_fm_amount) * density.cv,
      expo_fm_amount * density.cv,
      triggers,
      size);
  bool trigger = false;
  bool sustain = false;
  for (size_t i = 0; i < size; ++i) {
    trigger = trigger || triggers[i].trigger;
    sustain = sustain || freeze[i];
  }

  // A zone or polyphony change, once held, waits for main to configure.
  feedback_ = mix_parameters.feedback;
  RingsSynthTarget target;
  ComputeTarget(feedback_, polyphony_, &target);
  if (memcmp(&target, &wanted_, sizeof(target))) {
    wanted_ = target;
    settle_ = 0;
    pending_ = false;
  } else if (settle_ < kRingsSettleBlocks) {
    ++settle_;
  }
  if (settle_ == kRingsSettleBlocks && !pending_ &&
      memcmp(&wanted_, &configured_, sizeof(wanted_))) {
    pending_ = true;
  }

  ComputeControls(
      pitch,
      p.time.value(),
      p.size.value(),
      p.shape.value(),
      mix_parameters.dry_wet,
      sustain,
      trigger,
      seed_input != NULL,
      pitch_patched,
      &controls_);

  float l[kRingsBlockSize];
  float r[kRingsBlockSize];
  if (context_) {
    render_(context_, &controls_, l, r, size);
  } else {
    fill(&l[0], &l[size], 0.0f);
    fill(&r[0], &r[size], 0.0f);
  }

  // Quality as the other synths; then width from the sum to Rings' pair.
  const int bit_shift[4] = { 4, 6, 7, 16 };
  const float scale_up = float(1 << bit_shift[resolution]);
  const float scale_down = 1.0f / scale_up;
  const float side_gain = width_enabled_ ? stereo_width_ : 1.0f;
  for (size_t i = 0; i < size; ++i) {
    const float a = float(int32_t(l[i] * scale_up)) * scale_down * kRingsGain;
    const float b = float(int32_t(r[i] * scale_up)) * scale_down * kRingsGain;
    const float mid = 0.5f * (a + b);
    const float side = 0.5f * (a - b) * side_gain;
    out[i].l = mid + side;
    out[i].r = mid - side;
  }

  pre_reverb_limiter_.Process(out, size);
  const float reverb = mix_parameters.reverb_amount;
  const float reverb_mix = reverb * reverb * 0.8f;
#ifndef BEADS_FIXED_REVERB_DECAY
  const float reverb_time = reverb * reverb * reverb;
#endif  // BEADS_FIXED_REVERB_DECAY
  reverb_.set_mix(reverb_mix);
#ifdef BEADS_FIXED_REVERB_DECAY
  reverb_.set_time(Reverb::fixed_decay());
#else
  reverb_.set_time(0.3f + 0.6f * reverb_time);
#endif  // BEADS_FIXED_REVERB_DECAY
  reverb_.set_lp(0.6f);
  if (clouds_style_) {
    reverb_.ProcessClouds(out, size);
  } else {
    reverb_.Process(out, size);
  }
  output_limiter_.ProcessOutput(
      out, size, output_limiter_mode_, kSynthSampleRate);

  if (mix_parameters.output_gate) {
    for (size_t i = 0; i < size; ++i) {
      out[i].l = (out[i].l + out[i].r) * 0.5f;
      out[i].r = triggers[i].gate ? -0.8f : 0.0f;
    }
  }
}

#ifndef BEADS_FIXED_REVERB_DECAY
void RingsSynth::ComputeTarget(
    float feedback,
    int polyphony,
    RingsSynthTarget* target) {
  const int zone = zone_quantizer_.Process(feedback);
  if (zone < kNumRingsModels) {
    target_.core = RINGS_CORE_PART;
    target_.model = zone;
  } else {
    target_.core = RINGS_CORE_STRING_SYNTH;
    target_.fx = zone - kNumRingsModels;
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
#endif  // BEADS_FIXED_REVERB_DECAY

}  // namespace beads
