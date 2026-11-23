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
// Deferred gesture actions.

#include "beads/gesture_controller.h"

#include "stmlib/system/system_clock.h"

#include "beads/blob/synth_choice.h"
#include "beads/settings.h"
#include "beads/ui.h"

namespace beads {

using namespace stmlib;

/* static */
void GestureController::StepPlaitsBank(Ui* ui) {
#ifdef PLAITS_BLOBS
  const uint8_t bank = uint8_t(
      (ui->settings_->FeatureValue(FEATURE_PLAITS_BANK) + 1) %
          ui->settings_->profile().num_plaits_banks());
#else
  const uint8_t bank = uint8_t(
      (ui->settings_->FeatureValue(FEATURE_PLAITS_BANK) + 1) % kNumPlaitsBanks);
#endif  // PLAITS_BLOBS
  ui->settings_->SetFeatureValue(FEATURE_PLAITS_BANK, bank);
  ui->settings_->SaveOptions();
#ifdef PLAITS_BLOBS
  // Past three banks the colours repeat until Hygrostat's bank colours.
  ui->cue_ = ui->plaits_bank_cues_[bank % kNumPlaitsBanks];
#else
  ui->cue_ = ui->plaits_bank_cues_[bank];
#endif  // PLAITS_BLOBS
  ui->cue_time_ = system_clock.milliseconds();
}

/* static */
void GestureController::StepRingsPolyphony(Ui* ui) {
  const uint8_t polyphony = uint8_t(
      (ui->settings_->FeatureValue(FEATURE_RINGS_POLYPHONY) + 1) %
          kFeatureDefinitions[FEATURE_RINGS_POLYPHONY].values);
  ui->settings_->SetFeatureValue(FEATURE_RINGS_POLYPHONY, polyphony);
  ui->settings_->SaveOptions();
  // Rings' colours: green, yellow, red for one, two, four voices.
  ui->cue_.color = kOptionColors[polyphony];
  ui->cue_.pulses = PULSE_STEADY;
  ui->cue_time_ = system_clock.milliseconds();
}

/* static */
bool GestureController::DoAction(Ui* ui, uint8_t slot) {
  const ProfileFeature feature = ui->settings_->profile().Action(slot);
  // While Rings or Plaits plays, [A]+FREEZE steps its polyphony or bank
  // unless Synth start is there; otherwise the profile's action runs.
  if (slot == ACTION_FREEZE && ui->synth_mode_ && feature != FEATURE_SYNTH_START) {
    if (ui->playing_synth_ == SYNTH_RINGS) {
      StepRingsPolyphony(ui);
      return true;
    } else if (ui->playing_synth_ == SYNTH_PLAITS) {
      StepPlaitsBank(ui);
      return true;
    }
  }
  if (feature == FEATURE_MONOPHONIC) {
    State* state = ui->settings_->mutable_state();
    if (ui->settings_->profile().adjustable(feature)) {
      state->monophonic_mode = !state->monophonic_mode;
      ui->settings_->SaveState();
    }
    return true;
  } else if (feature == FEATURE_TRIM) {
    ui->trim_flag_ = true;
    ui->freeze_button_state_ = false;
    return true;
  } else if (feature == FEATURE_SYNTH_START &&
      ui->settings_->self_play_on_request() && !ui->spectral_ && ui->no_input_ &&
      !ui->should_ignore_state_change()) {
    ui->self_play_requested_ = true;
    return true;
  } else if (kFeatureDefinitions[feature].field != kNoField &&
      !kFeatureDefinitions[feature].stateless) {
    // Saved as from its options page, with that page's colour.
    const uint8_t current = ui->settings_->FeatureValue(feature);
    const uint8_t value = feature == FEATURE_SYNTH
        ? NextSynth(current, ui->playable_synths_) : !current;
    ui->settings_->SetFeatureValue(feature, value);
    ui->settings_->SaveOptions();
    const OptionValue cue = FindOptionValue(feature, value);
    ui->cue_.color = cue.color;
    ui->cue_.pulses = cue.pulses;
    ui->cue_time_ = system_clock.milliseconds();
    return true;
  }
  return false;
}

}  // namespace beads
