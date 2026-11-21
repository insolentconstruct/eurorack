// Copyright 2017 Emilie Gillet.
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
// The whole FX chain with the resonator and the profile's other options.

#include "beads/dsp/processor.h"

#include <algorithm>

namespace beads {

using namespace std;
using namespace stmlib;

void Processor::ProcessWithResonator(
    SeedButtonState seed_button_state,
    const stmlib::GateFlags* seed_input,
    const bool* freeze,
    const ModulatedParameter& density,
    const Parameters& input_parameters,
    const MixParameters& mix_parameters,
    const FloatFrame* in,
    FloatFrame* out,
    size_t size) {
  const float sr = sample_rate();
  Parameters scanned;
  const bool scan = (options_ & OPTION_FLAG_TIME_SCAN) && !spectral_ &&
      delay_mode_transition_ <= 0.0f && !input_parameters.time.has_cv;
  if (scan) {
    scanned = input_parameters;
    time_scan_.Process(float(size) / sr, &scanned.time);
  }
  const Parameters& parameters = scan ? scanned : input_parameters;
  const float delay_mode = delay_mode_transition_ > 0.0f;
  const float threshold = delay_mode ? 0.98f : 0.99f;
  const float direction = !spectral_ && delay_mode_ != DELAY_MODE_NEVER &&
      parameters.size.base > threshold ? 1.0f : -1.0f;
  delay_mode_transition_ += direction * float(size) / sr;

  CONSTRAIN(delay_mode_transition_,
            -kDelayModeTransitionTime,
            +kDelayModeTransitionTime);

  float expo_fm_amount = 20.0f * (fabsf(density.base - 0.5f) - 0.225f);
  CONSTRAIN(expo_fm_amount, 0.0f, 1.0f);

  TriggerGenerator::Output triggers[size];
  DelayController::Output taps[size];
  bool gate_out[size];

  // Tap gains at the start of the block, for the unity notch.
  const bool unity = (options_ & OPTION_FLAG_UNITY_FEEDBACK) && delay_mode;
  const bool synchronized = seed_button_state < SEED_BUTTON_STATE_IGNORE &&
      (seed_button_state != SEED_BUTTON_STATE_LOCKED || seed_input);
  const float tap_gain = unity
      ? delay_controller_.tap_gain(synchronized)
      : 1.0f;
  if (delay_mode) {
    const bool free_running = seed_button_state >= SEED_BUTTON_STATE_IGNORE;
    if (options_ & OPTION_FLAG_DELAY_CROSSFADE) {
      delay_controller_.RenderCrossfade(
          free_running ? SEED_BUTTON_STATE_LOCKED : seed_button_state,
          free_running ? NULL : seed_input,
          freeze,
          density.base,
          (1.0f - expo_fm_amount) * density.cv,
          expo_fm_amount * density.cv,
          parameters.time,
          parameters.shape,
          taps,
          size);
    } else {
      delay_controller_.Render(
          free_running ? SEED_BUTTON_STATE_LOCKED : seed_button_state,
          free_running ? NULL : seed_input,
          freeze,
          density.base,
          (1.0f - expo_fm_amount) * density.cv,
          expo_fm_amount * density.cv,
          parameters.time,
          parameters.shape,
          taps,
          size);
    }
    for (size_t i = 0; i < size; ++i) {
      gate_out[i] = taps[i].gate;
    }
  } else {
    trigger_generator_.Render(
        seed_button_state,
        seed_input,
        density.base,
        (1.0f - expo_fm_amount) * density.cv,
        expo_fm_amount * density.cv,
        triggers,
        size);
    AddAttackTrigger(triggers);
    for (size_t i = 0; i < size; ++i) {
      gate_out[i] = triggers[i].gate;
    }
  }
  const TriggerGenerator::Output* grain_triggers = delay_mode ? NULL : triggers;
  const DelayController::Output* delay_taps = delay_mode ? taps : NULL;

  FloatFrame dry_signal[size];
  const float feedback = mix_parameters.feedback;
  float loop_feedback = feedback;
  if (unity) {
    // The larger of the block's start and end, as feedback lags a block.
    loop_feedback = UnityFeedback(feedback, std::max(
        tap_gain, delay_controller_.tap_gain(synchronized)));
  }
  if (unity != bool(options_ & OPTION_FLAG_LOW_CUTOFF)) {
    SetFeedbackCutoff(unity);
  }
  if (spectral_) {
    QuantizeDrySignal(in, dry_signal, size);
    spectral_processor_.Process(
        seed_button_state, seed_input, freeze, density, parameters, feedback,
        dry_signal, out, size);
  } else {
    // Hits strike the resonator; read before the grains take their mark.
    const bool strike = (attack_trigger_ || AttackOnset());
    const FloatFrame* recorded = in;
    if (options_ & OPTION_FLAG_RESONATOR_INPUT) {
      copy(&in[0], &in[size], &resonated_[0]);
      ProcessResonator(parameters.pitch, strike, resonated_, size);
      recorded = resonated_;
    }
    switch (media_) {
      case MEDIA_DIGITAL:
        processor_digital_.Process(grain_triggers, delay_taps, freeze,
            parameters, loop_feedback, mono_, recorded, out, size);
        break;
      case MEDIA_VINTAGE_DIGITAL:
        processor_vintage_digital_.Process(grain_triggers, delay_taps, freeze,
            parameters, loop_feedback, mono_, recorded, out, size);
        break;
      case MEDIA_CLEAN_TAPE:
        processor_clean_tape_.Process(grain_triggers, delay_taps, freeze,
            parameters, loop_feedback, mono_, recorded, out, size);
        break;
      case MEDIA_DIRTY_TAPE:
        processor_dirty_tape_.Process(grain_triggers, delay_taps, freeze,
            parameters, loop_feedback, mono_, recorded, out, size);
        break;
    }
    QuantizeDrySignal(in, dry_signal, size);
    if (options_ & OPTION_FLAG_RESONATOR_GRAINS) {
      ProcessResonator(parameters.pitch, strike, out, size);
    }
    if (evolving_feedback_ || !feedback_diffuser_.idle()) {
      feedback_diffuser_.Process(
          out, feedback, evolving_feedback_ && freeze[size - 1], size);
    }
    if (diffusion_ > 0.0f && !delay_mode) {
      diffuser_.Process(out, size);
    }
  }

  const float wet_fade = SmoothStep(
      std::min(1.0f, fabsf(delay_mode_transition_) / kDelayModeFadeTime));

  // Dry/wet balance; the gentle law makes the wet side linear.
  const float post_gain_dry = 0.75f;
  const float post_gain_wet = 1.5f;
  const float dry_wet = mix_parameters.dry_wet;
  const float dry = (1.0f - dry_wet) * (1.0f + dry_wet) * post_gain_dry;
  const float wet_law = options_ & OPTION_FLAG_GENTLE_MIX
      ? dry_wet
      : dry_wet * (2.0f - dry_wet);
  const float wet = wet_law * post_gain_wet * wet_fade;
  if (options_ & OPTION_FLAG_REVERB_ON_GRAINS) {
    // Clouds' order: the reverb returns with the grains.
    pre_reverb_limiter_.Process(out, size);
    ProcessReverb(mix_parameters.reverb_amount, out, size);
    MixDryWet(dry_signal, dry, wet, out, size);
  } else {
    MixDryWet(dry_signal, dry, wet, out, size);
    pre_reverb_limiter_.Process(out, size);
    ProcessReverb(mix_parameters.reverb_amount, out, size);
  }

  output_limiter_.ProcessOutput(out, size, output_limiter_mode_, sr);

  if (mix_parameters.output_gate) {
    for (size_t i = 0; i < size; ++i) {
      out[i].l = (out[i].l + out[i].r) * 0.5f;
      out[i].r = gate_out[i] ? -0.8f : 0.0f;
    }
  }
}

void Processor::ProcessResonator(
    const RandomizedParameter& pitch,
    bool strike,
    FloatFrame* frames,
    size_t size) {
  const float sr = sample_rate();
  if (sr != resonator_sample_rate_) {
    resonator_.Init(sr);
    resonator_sample_rate_ = sr;
  }
  // PITCH and its CV, not its randomness; Recent notes plays the newest.
  float note = pitch.base;
  if (pitch.has_cv) {
    note += pitch.attenuverter >= 0.0f
        ? pitch.attenuverter * pitch.cv
        : NewestNote(0.0f);
  }
  const PitchScale scale = processor_digital_.pitch_scale();
  if (scale != PITCH_SCALE_OFF) {
    note = QuantizePitch(note, scale);
  }
  CONSTRAIN(note, -48.0f, 48.0f);
  resonator_.set_frequency(
      SemitonesToRatio(note + kResonatorRootNote - 69.0f) * 440.0f / sr);

  float excitation[size];
  for (size_t i = 0; i < size; ++i) {
    excitation[i] = (frames[i].l + frames[i].r) * kResonatorInputGain;
  }
  if (strike) {
    excitation[0] += kResonatorStrike;
  }
  resonator_.Render(excitation, frames, size);
}

bool Processor::AttackOnset() const {
  switch (media_) {
    case MEDIA_DIRTY_TAPE:
      return processor_dirty_tape_.attack_onset();
    case MEDIA_CLEAN_TAPE:
      return processor_clean_tape_.attack_onset();
    case MEDIA_VINTAGE_DIGITAL:
      return processor_vintage_digital_.attack_onset();
    default:
      return processor_digital_.attack_onset();
  }
}

}  // namespace beads
