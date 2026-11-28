// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct
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
// Profile gestures and their feedback.

#include "beads/gesture_controller.h"

#include "stmlib/system/system_clock.h"

#include "beads/blob/synth_choice.h"
#include "beads/gain_control.h"
#include "beads/settings.h"
#include "beads/ui.h"

namespace beads {

using namespace stmlib;

void GestureController::Init(Ui* ui, Settings* settings) {
  ui_ = ui;
  settings_ = settings;
  cue_time_ = 0;
  ui_->return_processor_ = PROCESSOR_GRANULAR;
  ui_->active_processor_ = settings_->processor_mode();
  ui_->processor_busy_ = false;
  ui_->processor_cue_pending_ = false;
  cue_ = plaits_bank_cues_[0];
  manual_delay_requested_ = false;
  trim_flag_ = false;
  self_play_requested_ = false;
}

void GestureController::LoadOptions() {
  const uint8_t amount_slots[MIX_PARAMETER_LAST] = {
    AMOUNT_FEEDBACK, AMOUNT_DRY_WET, AMOUNT_REVERB
  };
  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    amount_feature_[i] = settings_->profile().Amount(amount_slots[i]);
  }
}

bool GestureController::OnPressed(const Event& e, const Switches& switches) {
  if (switches.pressed(SWITCH_MEDIA) &&
      (e.control_id == SWITCH_SEED || e.control_id == SWITCH_FREEZE)) {
    bool pot_touched = false;
    for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
      pot_touched = pot_touched || \
          ui_->assignable_cv_pot_[i].editing_hidden_parameter();
    }
    DoAction(e.control_id == SWITCH_SEED ? ACTION_SEED : ACTION_FREEZE);
    for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
      ui_->assignable_cv_pot_[i].Unlock();
    }
    // The [A] release that saves the knob's edit is consumed here.
    if (pot_touched) {
      settings_->SaveState();
    }
    return true;
  }

  return false;
}

bool GestureController::OnReleased(const Event& e) {
  if (uint32_t(e.data) >= kSelfPlayHoldTime) {
    if (!DoAction(ACTION_HOLD)) {
      State* state = settings_->mutable_state();
      if (state->gain != 0) {
        state->gain = 0;
        settings_->SaveState();
      }
      ui_->gain_control_->Restart();
    }
    return true;
  }
  return false;
}

void GestureController::ProcessAmount(
    int i, PotController* p, uint32_t* press_time) {
  State* state = settings_->mutable_state();
  // Big hack to prevent the long press detection to fire!
  *press_time = system_clock.milliseconds();
  const ProfileFeature feature = amount_feature_[i];
  if (feature == FEATURE_GAIN_KNOB) {
    state->gain = 1.0f + p->hidden_parameter() * 255.0f;
  } else if (feature == FEATURE_WIDTH) {
    settings_->set_stereo_width(StereoWidthFromPot(
        p->hidden_parameter(), settings_->stereo_width()));
  } else if (feature == FEATURE_DIFFUSION) {
    settings_->set_diffusion(p->hidden_parameter());
  }
}

void GestureController::StepAssignment() {
  State* state = settings_->mutable_state();
  // Resonator damping comes last, then FEEDBACK again.
  if (ui_->resonator_assigned_) {
    ui_->resonator_assigned_ = false;
  } else if (settings_->width_assigned()) {
    // One store moves the target from width to diffusion.
    settings_->set_diffusion_assigned(true);
    if (!settings_->diffusion_assigned()) {
      settings_->set_width_assigned(false);
      ui_->resonator_assigned_ = ui_->resonator_available();
    }
  } else if (settings_->diffusion_assigned()) {
    settings_->set_diffusion_assigned(false);
    ui_->resonator_assigned_ = ui_->resonator_available();
  } else if (state->assignable_cv_destination == 2 && ui_->stereo_width_ &&
             settings_->profile().adjustable(FEATURE_WIDTH)) {
    settings_->set_width_assigned(true);
  } else if (state->assignable_cv_destination == 2 &&
             settings_->diffusion_assignable() &&
             settings_->profile().adjustable(FEATURE_DIFFUSION)) {
    settings_->set_diffusion_assigned(true);
  } else if (state->assignable_cv_destination == 2 && ui_->resonator_available()) {
    state->assignable_cv_destination = 0;
    ui_->resonator_assigned_ = true;
  } else {
    state->assignable_cv_destination =
        state->assignable_cv_destination == 3 ? 0
            : (state->assignable_cv_destination + 1) % 3;
  }
}

/* static */
const GestureController::BankCue
GestureController::plaits_bank_cues_[kNumPlaitsBanks] = {
  { { 0, 255, 0 }, PULSE_STEADY },
  { { 255, 0, 0 }, PULSE_STEADY },
  { { 255, 255, 0 }, PULSE_STEADY },
};

void GestureController::StepPlaitsBank() {
#ifdef PLAITS_BLOBS
  const uint8_t bank = uint8_t(
      (settings_->FeatureValue(FEATURE_PLAITS_BANK) + 1) %
          settings_->profile().num_plaits_banks());
#else
  const uint8_t bank = uint8_t(
      (settings_->FeatureValue(FEATURE_PLAITS_BANK) + 1) % kNumPlaitsBanks);
#endif  // PLAITS_BLOBS
  settings_->SetFeatureValue(FEATURE_PLAITS_BANK, bank);
  settings_->SaveOptions();
#ifdef PLAITS_BLOBS
  // Past three banks the colours repeat until Hygrostat's bank colours.
  cue_ = plaits_bank_cues_[bank % kNumPlaitsBanks];
#else
  cue_ = plaits_bank_cues_[bank];
#endif  // PLAITS_BLOBS
  cue_time_ = system_clock.milliseconds();
}

void GestureController::StepRingsPolyphony() {
  const uint8_t polyphony = uint8_t(
      (settings_->FeatureValue(FEATURE_RINGS_POLYPHONY) + 1) %
          kFeatureDefinitions[FEATURE_RINGS_POLYPHONY].values);
  settings_->SetFeatureValue(FEATURE_RINGS_POLYPHONY, polyphony);
  settings_->SaveOptions();
  // Rings' colours: green, yellow, red for one, two, four voices.
  cue_.color = kOptionColors[polyphony];
  cue_.pulses = PULSE_STEADY;
  cue_time_ = system_clock.milliseconds();
}

void GestureController::ObserveProcessor(ProcessorMode active, bool busy) {
  ui_->active_processor_ = active;
  ui_->processor_busy_ = busy;
  if (ui_->processor_cue_pending_ && !busy &&
      active == settings_->processor_mode()) {
    const uint32_t now = system_clock.milliseconds();
    // Let both warning flashes finish before showing the destination.
    if (cue_time_ && cue_.pulses == PULSE_DOUBLE &&
        uint32_t(now - cue_time_) < kOptionsFastPeriod * 3 / 2) {
      return;
    }
    const OptionValue cue = FindOptionValue(FEATURE_PROCESSOR, active);
    cue_.color = cue.color;
    cue_.pulses = cue.pulses;
    cue_time_ = now;
    ui_->processor_cue_pending_ = false;
  }
}

void GestureController::WarnProcessor() {
  const Leds::Color red = { 255, 0, 0 };
  cue_.color = red;
  cue_.pulses = PULSE_DOUBLE;
  cue_time_ = system_clock.milliseconds();
}

void GestureController::ToggleSpectral() {
  if (ui_->mode_ != Ui::MODE_NORMAL ||
      ui_->options_.stage() != Options::STAGE_INACTIVE ||
      ui_->options_.commit_requested() || ui_->freeze_button_state_ ||
      ui_->freeze_led_state_ || ui_->processor_busy_ ||
      ui_->active_processor_ != settings_->processor_mode()) {
    WarnProcessor();
    return;
  }

  const ProcessorMode next = ui_->active_processor_ == PROCESSOR_SPECTRAL
      ? ui_->return_processor_ : PROCESSOR_SPECTRAL;
  ui_->return_processor_ = next == PROCESSOR_SPECTRAL
      ? ui_->active_processor_ : PROCESSOR_GRANULAR;
  settings_->SetFeatureValue(FEATURE_PROCESSOR, next);
  settings_->SaveOptions();
  ui_->processor_cue_pending_ = true;
  cue_time_ = 0;
}

bool GestureController::DoAction(uint8_t slot) {
  const bool fallback = settings_->synth() == SYNTH_BEADS &&
      ui_->playing_synth_ == SYNTH_PLAITS &&
      !(ui_->playable_synths_ & (1 << SYNTH_BEADS));
  const ProfileFeature feature = fallback && slot == ACTION_HOLD
      ? FEATURE_SYNTH_START : settings_->profile().Action(slot);
  // A missing wavetable leaves the fallback bank and start gesture available.
  if (slot == ACTION_FREEZE && ui_->synth_mode_ &&
      (fallback || feature != FEATURE_SYNTH_START)) {
    if (ui_->playing_synth_ == SYNTH_RINGS) {
      StepRingsPolyphony();
      return true;
    } else if (ui_->playing_synth_ == SYNTH_PLAITS) {
      StepPlaitsBank();
      return true;
    }
  }
  if (feature == FEATURE_MONOPHONIC) {
    State* state = settings_->mutable_state();
    if (settings_->profile().adjustable(feature)) {
      state->monophonic_mode = !state->monophonic_mode;
      settings_->SaveState();
    }
    return true;
  } else if (feature == FEATURE_MANUAL_DELAY) {
    if (!ui_->synth_mode_ && ui_->processor_mode() == PROCESSOR_GRANULAR) {
      manual_delay_requested_ = !manual_delay_requested_;
      cue_.color = kOptionColors[manual_delay_requested_ ? 1 : 0];
      cue_.pulses = PULSE_STEADY;
      cue_time_ = system_clock.milliseconds();
    }
    return true;
  } else if (feature == FEATURE_TRIM) {
    trim_flag_ = true;
    ui_->freeze_button_state_ = false;
    return true;
  } else if (feature == FEATURE_SYNTH_START &&
      settings_->self_play_on_request() && ui_->no_input_ &&
      ui_->processor_mode() == PROCESSOR_GRANULAR &&
      !ui_->should_ignore_state_change()) {
    self_play_requested_ = true;
    return true;
  } else if (feature == FEATURE_PROCESSOR) {
    ToggleSpectral();
    return true;
  } else if (kFeatureDefinitions[feature].field != kNoField &&
      !kFeatureDefinitions[feature].stateless) {
    // Saved as from its options page, with that page's colour.
    // A later gesture acts on the pending page value and replaces its edit.
    const uint8_t current = ui_->options_.edited(feature)
        ? ui_->options_.Value(feature) : settings_->FeatureValue(feature);
    const uint8_t value = feature == FEATURE_SYNTH
        ? NextSynth(current, ui_->playable_synths_) : !current;
    settings_->SetFeatureValue(feature, value);
    ui_->options_.PublishGesture(feature, value);
    settings_->SaveOptions();
    const OptionValue cue = FindOptionValue(feature, value);
    cue_.color = cue.color;
    cue_.pulses = cue.pulses;
    cue_time_ = system_clock.milliseconds();
    return true;
  }
  return false;
}

}  // namespace beads
