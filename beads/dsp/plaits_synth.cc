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
// Six Plaits voices, played in place of the wavetable synth.

#include "beads/dsp/plaits_synth.h"

#include <algorithm>

#include "stmlib/utils/random.h"

namespace beads {

using namespace std;
using namespace stmlib;

// 10 Vpp per voice in the centre, as the wavetable synth's dry oscillator.
const float kPlaitsVoiceGain = 2.0f / 3.0f;
const float kModelHysteresis = 0.048f;

void PlaitsSynth::Init(uint16_t* reverb_buffer) {
  clouds_style_ = false;
  output_limiter_mode_ = OUTPUT_LIMITER_STOCK;
  const float sr = kSynthSampleRate;

  reverb_.Init(reverb_buffer, sr);

  trigger_generator_.Init(sr);
  pre_reverb_limiter_.Init(1.0f / (sr * 4.0f), 0.9f);
  output_limiter_.Init(1.0f / (sr * 6.0f), 0.9f);
  harmonics_modulation_.Init(1.01f / sr);
  timbre_modulation_.Init(1.01f / sr);
  morph_modulation_.Init(1.01f / sr);
  model_quantizer_.Init(kNumPlaitsModels, kModelHysteresis, false);

  stereo_width_ = 0.0f;
  width_enabled_ = false;
  pitch_scale_ = PITCH_SCALE_OFF;
}

void PlaitsSynth::InitVoices(uint8_t* ram) {
  for (int i = 0; i < kNumPlaitsVoices; ++i) {
    voice_[i].Init(ram + i * kPlaitsVoiceRamSize);

    PlaitsPatch& patch = patch_[i];
    patch.model = model_quantizer_.quantized_value();
    patch.note = kPlaitsRootNote;
    patch.harmonics = patch.timbre = patch.morph = 0.5f;
    gain_l_[i] = gain_r_[i] = kPlaitsVoiceGain * 0.75f;
  }
  active_voice_ = 0;
  noon_ = false;
  drone_ = false;
}

void PlaitsSynth::StartNote(const Parameters& p, int model) {
  active_voice_ = (active_voice_ + 1) % kNumPlaitsVoices;

  // Each note keeps the settings and random values it started with.
  PlaitsPatch& patch = patch_[active_voice_];
  float pitch = p.pitch.value(NULL, 0, 24.0f, false);
  if (pitch_scale_ != PITCH_SCALE_OFF) {
    pitch = QuantizePitch(pitch, pitch_scale_);
  }
  patch.model = model;
  patch.note = kPlaitsRootNote + pitch;
  patch.harmonics = p.time.value();
  patch.timbre = p.size.value();
  patch.morph = p.shape.value();

  const float pan = width_enabled_
      ? 0.5f + stereo_width_ * (Random::GetFloat() - 0.5f)
      : 0.5f;
  gain_l_[active_voice_] = kPlaitsVoiceGain * (1.0f - pan) * (1.0f + pan);
  gain_r_[active_voice_] = kPlaitsVoiceGain * pan * (2.0f - pan);
}

void PlaitsSynth::Render(
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
  p.pitch.base = p.pitch.base + (p.pitch.has_cv ? p.pitch.cv : 0.0f);
  p.pitch.has_cv = false;

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
  bool gate = false;
  bool sustain = false;
  for (size_t i = 0; i < size; ++i) {
    trigger = trigger || triggers[i].trigger;
    gate = gate || triggers[i].gate;
    sustain = sustain || freeze[i];
  }

  // DENSITY at noon with SEED unpatched drones, as Plaits with TRIG
  // unpatched, until a SEED press plays a note.
  float d = density.base + (1.0f - expo_fm_amount) * density.cv;
  CONSTRAIN(d, 0.0f, 1.0f);
  const bool noon = !seed_input && d >= 0.48f && d <= 0.52f;
  if (noon && !noon_) {
    drone_ = true;
  }
  noon_ = noon;
  drone_ = drone_ && noon && !trigger;

  const int model = model_quantizer_.Process(mix_parameters.feedback);
  if (trigger) {
    StartNote(p, model);
  } else if (drone_) {
    PlaitsPatch& patch = patch_[active_voice_];
    float pitch = p.pitch.base;
    if (pitch_scale_ != PITCH_SCALE_OFF) {
      pitch = QuantizePitch(pitch, pitch_scale_);
    }
    patch.model = model;
    patch.note = kPlaitsRootNote + pitch;
    patch.harmonics = p.time.value(&harmonics_modulation_, size, 1.0f, true);
    patch.timbre = p.size.value(&timbre_modulation_, size, 1.0f, true);
    patch.morph = p.shape.value(&morph_modulation_, size, 1.0f, true);
  }

  const int bit_shift[4] = { 4, 6, 7, 16 };

  FloatFrame zero;
  zero = 0.0f;
  fill(&out[0], &out[size], zero);
  for (int v = 0; v < kNumPlaitsVoices; ++v) {
    const bool active = v == active_voice_;
    const int voice_trigger = active && drone_
        ? plaits::TRIGGER_UNPATCHED
        : (active && trigger ? plaits::TRIGGER_RISING_EDGE : 0) |
            ((active && gate) || sustain ? plaits::TRIGGER_HIGH : 0);
    float voice_out[size];
    voice_[v].Render(
        patch_[v],
        voice_trigger,
        sustain,
        mix_parameters.dry_wet,
        bit_shift[resolution],
        voice_out,
        size);
    const float gain_l = gain_l_[v];
    const float gain_r = gain_r_[v];
    for (size_t i = 0; i < size; ++i) {
      out[i].l += voice_out[i] * gain_l;
      out[i].r += voice_out[i] * gain_r;
    }
  }

  pre_reverb_limiter_.Process(out, size);

  // Reverb.
  const float reverb = mix_parameters.reverb_amount;
  const float reverb_mix = reverb * reverb * 0.8f;
  const float reverb_time = reverb * reverb * reverb;
  reverb_.set_mix(reverb_mix);
  reverb_.set_time(0.3f + 0.6f * reverb_time);
  reverb_.set_lp(0.6f);
  if (clouds_style_) {
    reverb_.ProcessClouds(out, size);
  } else {
    reverb_.Process(out, size);
  }

  // Final clipping/limiting
  output_limiter_.ProcessOutput(
      out, size, output_limiter_mode_, kSynthSampleRate);

  if (mix_parameters.output_gate) {
    for (size_t i = 0; i < size; ++i) {
      out[i].l = (out[i].l + out[i].r) * 0.5f;
      out[i].r = triggers[i].gate ? -0.8f : 0.0f;
    }
  }
}

}  // namespace beads
