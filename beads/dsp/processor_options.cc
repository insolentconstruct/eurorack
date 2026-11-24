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
// The whole FX chain, with TIME scan and the profile's options.

#include "beads/dsp/processor.h"

#include <algorithm>

#include "beads/dsp/fx/reverb_decay_render.h"

namespace beads {

using namespace std;
using namespace stmlib;

template<typename DelayLineType>
void DelayGranularPlayer::RenderSend(
    const DelayLineType* delay_line,
    const TriggerGenerator::Output& trigger,
    const Parameters& p,
    float gain,
    FloatFrame* out) {
  if (trigger.trigger && gain > 0.0f) {
    const float duration = p.size.value();
    const bool reverse = duration < 0.3333333f;

    float amplitude, pan;
    ComputeGrainAmplitudeAndPan(trigger, p, &amplitude, &pan);
    amplitude *= gain;

    const int safety = DelayGrain::head_crossing_safety_zone + 1;
    const int max_delay = delay_line->duration() - 2 * safety;

    new_grain()->Start(
        delay_line,
        attacks_ ? attacks_->Snap(
            p.time.value() * float(max_delay) + float(safety),
            float(max_delay + safety), delay_line->write_ptr())
            : p.time.value() * float(max_delay) + float(safety),
        (reverse ? -1.0f : 1.0f) * \
            p.pitch.pitch_value(pitch_scale_, random_intervals_),
        ComputeGrainDuration(duration),
        p.shape.value(),
        amplitude, pan,
        trigger.fractional_sample);
    SortGrainList(p.monophonic_mode);
  }

  // Sum the grains in a local frame, which the compiler can keep in
  // registers.
  FloatFrame sum;
  sum.l = 0.0f;
  sum.r = 0.0f;
  for (int i = 0; i < NUM_GRAINS; ++i) {
    grain_[i].Render<HEAD_CROSSING_POLICY_STOP, true>(delay_line, &sum);
  }
  *out += sum;

  Tick();  // Required to estimate the timing between grains.
}

template<Resolution resolution, int max_delay, int ratio>
void GranularProcessor<resolution, max_delay, ratio>::ProcessSend(
    const TriggerGenerator::Output* triggers,
    const DelayController::Output* taps,
    const bool* freeze,
    const Parameters& parameters,
    const float feedback,
    const bool mono,
    const FloatFrame* in,
    FloatFrame* out,
    size_t size, float gain) {
  if (ratio == 1) {
    ProcessSendInternal(
        triggers, taps, freeze, parameters, feedback,
        mono, in, out, size, gain);
  } else {
    size_t d_size = size / size_t(ratio);

    bool d_freeze[d_size];
    TriggerGenerator::Output d_triggers[d_size];
    DelayController::Output d_taps[d_size];
    FloatFrame d_in[d_size];
    FloatFrame d_out[d_size];
    float downsampled[d_size];
    float upsampled[size];

    DownsampleGate(freeze, d_freeze, size);

    if (triggers) {
      DownsampleTriggers(triggers, d_triggers, size);
    }
    if (taps) {
      DownsampleTaps(taps, d_taps, size);
    }

    for (size_t channel = 0; channel < 2; ++channel) {
      for (size_t i = 0; i < size; ++i) {
        upsampled[i] = *(&(in[i].l) + channel);
      }
      src_down_[channel].Process(upsampled, downsampled, size);
      for (size_t i = 0; i < d_size; ++i) {
        *(&(d_in[i].l) + channel) = downsampled[i];
      }
    }

    ProcessSendInternal(
        triggers ? d_triggers : NULL,
        taps ? d_taps : NULL,
        d_freeze,
        parameters, feedback,
        mono, d_in, d_out, d_size, gain);

    for (size_t channel = 0; channel < 2; ++channel) {
      for (size_t i = 0; i < d_size; ++i) {
        downsampled[i] = *(&(d_out[i].l) + channel);
      }
      src_up_[channel].Process(downsampled, upsampled, d_size);
      for (size_t i = 0; i < size; ++i) {
        *(&(out[i].l) + channel) = upsampled[i];
      }
    }
  }
}

template<Resolution resolution, int max_delay, int ratio>
void GranularProcessor<resolution, max_delay, ratio>::ProcessSendInternal(
    const TriggerGenerator::Output* grain_triggers,
    const DelayController::Output* taps,
    const bool* freeze,
    const Parameters& grain_parameters,
    const float feedback,
    const bool mono,
    const FloatFrame* in,
    FloatFrame* out,
    size_t size, float gain) {
  if (mono) {
    ProcessSendInternal<true>(
        grain_triggers, taps, freeze, grain_parameters, feedback,
        in, out, size, gain);
  } else {
    ProcessSendInternal<false>(
        grain_triggers, taps, freeze, grain_parameters, feedback,
        in, out, size, gain);
  }
}

template<Resolution resolution, int max_delay, int ratio>
template<bool mono>
void GranularProcessor<resolution, max_delay, ratio>::ProcessSendInternal(
    const TriggerGenerator::Output* grain_triggers,
    const DelayController::Output* taps,
    const bool* freeze,
    const Parameters& grain_parameters,
    const float feedback,
    const FloatFrame* in,
    FloatFrame* out,
    size_t size, float gain) {
  const bool wide_detent = filter_mode_.wide_detent;
  float q_offset = wide_detent ? kWideDetentQOffset : kStockDelayQOffset;
  float q_scale = wide_detent ? 0.0f : kStockDelayQScale;
  // volatile stops GCC 9 fusing this into an fmaf, which rounds
  // differently; -ffp-contract=off would change its other choices.
  const volatile float speed = stmlib::SemitonesToRatio(
      grain_parameters.pitch.value(&pitch_modulation_, size, 24.0f, false));

  float bandwidth = 0.5f * (1.0f + grain_parameters.size.attenuverter) + \
      (grain_parameters.size.has_cv ? grain_parameters.size.cv : 0.0f);
  CONSTRAIN(bandwidth, 0.0f, 1.0f)

  // Mode and ramp position share one word; zero means settled Auto, so
  // stock playback skips coefficient work with a single test.
  uint32_t filter_mode;
  memcpy(&filter_mode, &filter_mode_, sizeof(filter_mode));
  if (filter_mode) {
    const DelayFilterQ q = SmoothDelayFilterQ(
        &filter_mode_, q_offset, q_scale);
    q_offset = q.offset;
    q_scale = q.scale;
    if (wide_detent) {
      bandwidth = DelayFilterDetent(bandwidth);
    }
  }
  stmlib::ParameterInterpolator bandwidth_modulation(
      &bandwidth_, bandwidth, size);
  stmlib::ParameterInterpolator feedback_modulation(
      &feedback_, feedback, size);
  stmlib::ParameterInterpolator speed_modulation(&speed_, speed, size);

  const float feedback_gain = taps ? 1.5f : 3.0f;

  const float lp_coefficients[RESOLUTION_LAST] = { 0.75f, 0.9f, 0.999f };
  const float input_gains[RESOLUTION_LAST] = { 1.75f, 1.3f, 0.0f };

  const float lp_coefficient = lp_coefficients[resolution];
  const float soft_limit_gain = input_gains[resolution];

  // Attack marks follow the recording, in samples written.
  AttackDetector* const attacks = attacks_.mode() ? &attacks_ : NULL;
  granular_player_.SetAttacks(attacks);
  uint32_t writes = 0;
  if (attacks) {
    for (size_t i = 0; i < size; ++i) {
      writes += !freeze[i];
    }
    attacks->BeginBlock(
        mono ? mono_delay_line_.write_ptr() : stereo_delay_line_.write_ptr(),
        mono ? max_delay * 2 : max_delay,
        grain_triggers != NULL,
        freeze[0]);
  }
  while (size--) {
    const float feedback = feedback_modulation.Next();
    FloatFrame input = *in;
    FloatFrame s = input;

    if (soft_limit_gain) {
      const float gain = feedback_gain / soft_limit_gain;
      s += previous_sample_ * feedback * gain;
    } else {
      s += previous_sample_ * feedback * feedback * feedback_gain;
    }
    input_limiter_.Process(&s, 1);

    if (soft_limit_gain) {
      s *= soft_limit_gain;
      s.l = stmlib::SoftClip(s.l);
      s.r = stmlib::SoftClip(s.r);
      s = input + feedback * (s - input);
    }

    out->l = 0.0f;
    out->r = 0.0f;
    if (mono) {
      mono_delay_line_.Write(s, *freeze);
      if (grain_triggers) {
        granular_player_.RenderSend(
            &mono_delay_line_, *grain_triggers, grain_parameters, gain, out);
      }
      if (taps) {
        multitap_delay_.Render(
            &mono_delay_line_,
            *taps,
            speed_modulation.Next(),
            bandwidth_modulation.Next(),
            q_offset, q_scale,
            out);
      }
    } else {
      stereo_delay_line_.Write(s, *freeze);
      if (grain_triggers) {
        granular_player_.RenderSend(
            &stereo_delay_line_, *grain_triggers,
            grain_parameters, gain, out);
      }
      if (taps) {
        multitap_delay_.Render(
            &stereo_delay_line_,
            *taps,
            speed_modulation.Next(),
            bandwidth_modulation.Next(),
            q_offset, q_scale,
            out);
      }
    }

    ONE_POLE(
        previous_sample_.l,
        dc_blocker_[0].template Process<stmlib::FILTER_MODE_HIGH_PASS>(
            out->l),
        lp_coefficient);
    ONE_POLE(
        previous_sample_.r,
        dc_blocker_[1].template Process<stmlib::FILTER_MODE_HIGH_PASS>(
            out->r),
        lp_coefficient);

    if (grain_triggers) {
      ++grain_triggers;
    }
    if (taps) {
      ++taps;
    }

    ++freeze;
    ++in;
    ++out;
  }
  if (attacks) {
    attacks->EndBlock(writes);
  }
}

#ifndef BEADS_FIXED_REVERB_DECAY
// 1.5 x feedback squared is the delay loop gain of one tap.
const float kUnityFeedback = 0.8164966f;
const float kUnityNotchWidth = 0.03f;

/* static */
float Processor::UnityFeedback(float feedback, float tap_gain) {
  const float unity = kUnityFeedback / Sqrt(tap_gain);
  const float low = kUnityFeedback - kUnityNotchWidth;
  const float high = kUnityFeedback + kUnityNotchWidth;
  if (feedback < low) {
    return feedback * (unity / low);
  } else if (feedback > high) {
    return unity + (feedback - high) * ((1.0f - unity) / (1.0f - high));
  } else {
    return unity;
  }
}

void Processor::SetFeedbackCutoff(bool low) {
  const float cutoff = low ? 10.0f : 40.0f;
  processor_digital_.set_feedback_cutoff(cutoff / 48000.0f);
  processor_vintage_digital_.set_feedback_cutoff(cutoff / 32000.0f);
  processor_clean_tape_.set_feedback_cutoff(cutoff / 24000.0f);
  processor_dirty_tape_.set_feedback_cutoff(cutoff / 24000.0f);
  SetOption(OPTION_FLAG_LOW_CUTOFF, low);
}
#endif  // BEADS_FIXED_REVERB_DECAY

template<bool resonator, bool manual_delay>
void Processor::BEADS_DECAY_RENDER(ProcessOptions)(
    bool delay_requested,
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
  const float transition_direction = manual_delay
      ? (!spectral_ && delay_requested ? 1.0f : -1.0f) : direction;
  delay_mode_transition_ += transition_direction * float(size) / sr;

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

  const bool send = (options_ & OPTION_FLAG_SEND_MIX) &&
      !delay_mode && !spectral_;
  FloatFrame dry_signal[size];
  const float feedback = resonator && resonator_controls_.hold_feedback
      ? resonator_controls_.feedback : mix_parameters.feedback;
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
    if (resonator) {
      // No old tail when the grains return.
      resonator_sample_rate_ = 0.0f;
    }
  } else {
    // Hits, read before the grains take their mark, and SEED strike it;
    // in delay mode SEED is the clock.
    const bool seed = resonator &&
        SeedStrike(seed_button_state, seed_input, size) && !delay_mode;
    const bool strike = resonator &&
        (seed || attack_trigger_ || AttackOnset());
    FloatFrame resonated[resonator ? size : 1];
    const FloatFrame* recorded = in;
    if (resonator && (options_ & OPTION_FLAG_RESONATOR_INPUT)) {
      // Both flags: in the feedback.
      if (options_ & OPTION_FLAG_RESONATOR_GRAINS) {
        loop_feedback = RecordResonance(
            loop_feedback, delay_mode, in, resonated, size);
      } else {
        copy(&in[0], &in[size], &resonated[0]);
        ProcessResonator(parameters.pitch, strike, resonated, size);
      }
      recorded = resonated;
    }
    switch (media_) {
      case MEDIA_DIGITAL:
        if (send) {
          processor_digital_.ProcessSend(
              grain_triggers, delay_taps, freeze, parameters, loop_feedback,
              mono_, recorded, out, size, mix_parameters.dry_wet);
        } else {
          processor_digital_.Process(grain_triggers, delay_taps, freeze,
              parameters, loop_feedback, mono_, recorded, out, size);
        }
        break;
      case MEDIA_VINTAGE_DIGITAL:
        if (send) {
          processor_vintage_digital_.ProcessSend(
              grain_triggers, delay_taps, freeze, parameters, loop_feedback,
              mono_, recorded, out, size, mix_parameters.dry_wet);
        } else {
          processor_vintage_digital_.Process(grain_triggers, delay_taps, freeze,
              parameters, loop_feedback, mono_, recorded, out, size);
        }
        break;
      case MEDIA_CLEAN_TAPE:
        if (send) {
          processor_clean_tape_.ProcessSend(
              grain_triggers, delay_taps, freeze, parameters, loop_feedback,
              mono_, recorded, out, size, mix_parameters.dry_wet);
        } else {
          processor_clean_tape_.Process(grain_triggers, delay_taps, freeze,
              parameters, loop_feedback, mono_, recorded, out, size);
        }
        break;
      case MEDIA_DIRTY_TAPE:
        if (send) {
          processor_dirty_tape_.ProcessSend(
              grain_triggers, delay_taps, freeze, parameters, loop_feedback,
              mono_, recorded, out, size, mix_parameters.dry_wet);
        } else {
          processor_dirty_tape_.Process(grain_triggers, delay_taps, freeze,
              parameters, loop_feedback, mono_, recorded, out, size);
        }
        break;
    }
    QuantizeDrySignal(in, dry_signal, size);
    if (resonator && (options_ & OPTION_FLAG_RESONATOR_GRAINS)) {
      if (options_ & OPTION_FLAG_RESONATOR_INPUT) {
        ProcessResonatorLoop(parameters.pitch, strike, out, size);
      } else {
        ProcessResonator(parameters.pitch, strike, out, size);
      }
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
  const float dry_wet = send ? 0.0f : mix_parameters.dry_wet;
  const float dry = (1.0f - dry_wet) * (1.0f + dry_wet) * post_gain_dry;
  const float wet_law = send ? 1.0f : options_ & OPTION_FLAG_GENTLE_MIX
      ? dry_wet
      : dry_wet * (2.0f - dry_wet);
  const float wet = wet_law * post_gain_wet * wet_fade;
  if (send || (options_ & OPTION_FLAG_REVERB_ON_GRAINS)) {
    // Clouds' order: the reverb returns with the grains.
    pre_reverb_limiter_.Process(out, size);
    BEADS_DECAY_RENDER(ProcessReverb)(mix_parameters.reverb_amount, out, size);
    MixDryWet(dry_signal, dry, wet, out, size);
  } else {
    MixDryWet(dry_signal, dry, wet, out, size);
    pre_reverb_limiter_.Process(out, size);
    BEADS_DECAY_RENDER(ProcessReverb)(mix_parameters.reverb_amount, out, size);
  }

  output_limiter_.ProcessOutput(out, size, output_limiter_mode_, sr);

  if (mix_parameters.output_gate) {
    if ((options_ & OPTION_FLAG_OUTPUT_DIVISION) && !delay_mode && !spectral_) {
      DivideOutput(triggers, gate_out, size);
    }
    for (size_t i = 0; i < size; ++i) {
      out[i].l = (out[i].l + out[i].r) * 0.5f;
      out[i].r = gate_out[i] ? -0.8f : 0.0f;
    }
  }
}

#ifndef BEADS_FIXED_REVERB_DECAY
void Processor::set_output_division(int division) {
  CONSTRAIN(division, 1, 16);
  // Init clears the flag, so the first configuration resets the divider.
  if (!(options_ & OPTION_FLAG_OUTPUT_DIVISION) ||
      division != output_division_) {
    output_division_ = division;
    output_pulse_count_ = 0;
    output_pulse_ = false;
  }
  SetOption(OPTION_FLAG_OUTPUT_DIVISION, division > 1);
}

void Processor::DivideOutput(
    const TriggerGenerator::Output* triggers, bool* gate, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    if (triggers[i].trigger) {
      output_pulse_ = output_pulse_count_ == 0;
      if (++output_pulse_count_ >= output_division_) {
        output_pulse_count_ = 0;
      }
    }
    gate[i] = triggers[i].gate && output_pulse_;
  }
}
#endif  // BEADS_FIXED_REVERB_DECAY

void Processor::BEADS_DECAY_RENDER(ProcessWithOptions)(
    SeedButtonState seed_button_state,
    const stmlib::GateFlags* seed_input,
    const bool* freeze,
    const ModulatedParameter& density,
    const Parameters& parameters,
    const MixParameters& mix_parameters,
    const FloatFrame* in,
    FloatFrame* out,
    size_t size) {
  BEADS_DECAY_RENDER(ProcessOptionsBlock)<false>(false, seed_button_state,
      seed_input, freeze, density, parameters, mix_parameters, in, out, size);
}

void Processor::BEADS_DECAY_RENDER(ProcessManualDelayWithOptions)(
    bool delay_requested,
    SeedButtonState seed_button_state,
    const stmlib::GateFlags* seed_input,
    const bool* freeze,
    const ModulatedParameter& density,
    const Parameters& parameters,
    const MixParameters& mix_parameters,
    const FloatFrame* in,
    FloatFrame* out,
    size_t size) {
  BEADS_DECAY_RENDER(ProcessOptionsBlock)<true>(delay_requested,
      seed_button_state, seed_input, freeze, density, parameters,
      mix_parameters, in, out, size);
}

template<bool manual_delay>
void Processor::BEADS_DECAY_RENDER(ProcessOptionsBlock)(
    bool delay_requested,
    SeedButtonState seed_button_state,
    const stmlib::GateFlags* seed_input,
    const bool* freeze,
    const ModulatedParameter& density,
    const Parameters& parameters,
    const MixParameters& mix_parameters,
    const FloatFrame* in,
    FloatFrame* out,
    size_t size) {
  // Resonator profiles run their own instance of the block.
  if (options_ &
      (OPTION_FLAG_RESONATOR_GRAINS | OPTION_FLAG_RESONATOR_INPUT)) {
    BEADS_DECAY_RENDER(ProcessOptions)<true, manual_delay>(
        delay_requested, seed_button_state, seed_input, freeze, density, parameters,
        mix_parameters, in, out, size);
  } else {
    BEADS_DECAY_RENDER(ProcessOptions)<false, manual_delay>(
        delay_requested, seed_button_state, seed_input, freeze, density, parameters,
        mix_parameters, in, out, size);
  }
}

#ifndef BEADS_FIXED_REVERB_DECAY
void Processor::MixDryWet(
    const FloatFrame* dry_signal,
    float dry,
    float wet,
    FloatFrame* out,
    size_t size) {
  stmlib::ParameterInterpolator dry_modulation(&dry_, dry, size);
  stmlib::ParameterInterpolator wet_modulation(&wet_, wet, size);
  for (size_t i = 0; i < size; ++i) {
    out[i] = dry_modulation.Next() * dry_signal[i] +
        wet_modulation.Next() * out[i];
  }
  // Wow and flutter simulation.
  if (media_ == MEDIA_DIRTY_TAPE) {
    wow_and_flutter_.Process(out, size);
  }
}
#endif  // BEADS_FIXED_REVERB_DECAY

void Processor::BEADS_DECAY_RENDER(ProcessReverb)(
    float reverb, FloatFrame* out, size_t size) {
  const bool caveman = media_ != MEDIA_DIGITAL;
  reverb_.set_mix(reverb * reverb * 0.9f);
#ifndef BEADS_FIXED_REVERB_DECAY
  const float reverb_time = reverb * reverb * reverb;
#endif  // BEADS_FIXED_REVERB_DECAY
#ifdef BEADS_FIXED_REVERB_DECAY
  reverb_.set_time(Reverb::fixed_decay());
#else
  reverb_.set_time(caveman
      ? (0.45f + 0.5f * reverb_time)
      : (0.3f + 0.6f * reverb_time));
#endif  // BEADS_FIXED_REVERB_DECAY
  reverb_.set_lp(
      caveman ? 0.4f : (media_ == MEDIA_VINTAGE_DIGITAL ? 0.75f : 0.6f));
  reverb_.Render(out, size);
}

}  // namespace beads
