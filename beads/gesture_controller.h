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

#ifndef BEADS_GESTURE_CONTROLLER_H_
#define BEADS_GESTURE_CONTROLLER_H_

#include "stmlib/ui/event_queue.h"

#include "beads/options.h"
#include "beads/drivers/switches.h"
#include "beads/dsp/plaits_synth.h"

namespace beads {

class PotController;
class Settings;
class Ui;

class GestureController {
 public:
  GestureController() { }
  ~GestureController() { }

  struct BankCue {
    Leds::Color color;
    PulsePattern pulses;
  };

  void Init(Ui* ui, Settings* settings);
  void LoadOptions();
  bool OnPressed(const stmlib::Event& e, const Switches& switches);
  bool OnReleased(const stmlib::Event& e);
  void ProcessAmount(int i, PotController* p, uint32_t* press_time);
  void StepAssignment();

  inline const BankCue& cue() const { return cue_; }
  inline uint32_t cue_time() const { return cue_time_; }
  inline bool manual_delay_requested() const { return manual_delay_requested_; }
  inline bool trim_flag() const { return trim_flag_; }
  inline bool self_play_requested() const { return self_play_requested_; }
  inline void ClearTrimFlag() { trim_flag_ = false; }
  inline void ClearSelfPlayRequest() { self_play_requested_ = false; }
  inline ProfileFeature amount_feature(int i) const {
    return amount_feature_[i];
  }
  inline bool media_amount(int i) const {
    return amount_feature_[i] != FEATURE_NONE;
  }

 private:
  friend class Ui;

  void StepPlaitsBank();
  void StepRingsPolyphony();
  bool DoAction(uint8_t slot);

  Ui* ui_;
  Settings* settings_;
  BankCue cue_;
  uint32_t cue_time_;
  ProfileFeature amount_feature_[MIX_PARAMETER_LAST];
  volatile bool manual_delay_requested_;
  bool trim_flag_;
  bool self_play_requested_;
  static const BankCue plaits_bank_cues_[kNumPlaitsBanks];

  DISALLOW_COPY_AND_ASSIGN(GestureController);
};

}  // namespace beads

#endif  // BEADS_GESTURE_CONTROLLER_H_
