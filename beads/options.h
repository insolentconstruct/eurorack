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
// Guarded options page.

#ifndef BEADS_OPTIONS_H_
#define BEADS_OPTIONS_H_

#include <cmath>

#include "stmlib/stmlib.h"
#include "beads/drivers/leds.h"
#include "beads/dsp/limiter.h"
#include "beads/dsp/attack_detector.h"
#include "beads/dsp/playback_options.h"
#include "beads/dsp/pitch_quantizer.h"

namespace beads {

class Profile;

class Settings;

const int MIX_PARAMETER_LAST = 3;
const int kNumOptionPages = 4;

enum OptionSetting {
  OPTION_RESERVED,
  OPTION_PITCH,
  OPTION_REVERB,
  OPTION_DELAY,
  OPTION_PROCESSOR,
  OPTION_LIMITER,
  OPTION_FROZEN,
  OPTION_SYNTH,
  OPTION_ATTACKS,
  OPTION_SYNTH_START,
  OPTION_INTERVALS,
  OPTION_LAST,
};

// Pages light the mix LEDs left to right; the last page lights all three.
struct OptionPage {
  int led_index;  // MIX_PARAMETER_LAST for all three.
  OptionSetting knobs[MIX_PARAMETER_LAST];  // FEEDBACK, DRY/WET, REVERB.
};
const int kPitchScalePage = 0;
const uint32_t kOptionsHoldTime = 3000;
const uint32_t kOptionsTapTime = 500;
const uint32_t kOptionsArmTime = 20;
const float kOptionsPickupThreshold = 0.10f;
const float kOptionsMovementThreshold = 0.005f;
const float kWidthStockThreshold = 0.07f;
const float kWidthEnabledThreshold = 0.13f;
const float kWidthRange = 0.87f;
const float kOptionsHalfLow = 0.47f;
const float kOptionsHalfHigh = 0.53f;
const float kOptionsZoneLow = 1.0f / 3.0f;
const float kOptionsZoneHigh = 2.0f / 3.0f;
const float kOptionsZoneBand = 0.03f;
const uint32_t kOptionsSettleTime = 1000;
const uint32_t kOptionsFlashTime = 100;
const uint32_t kOptionsSlowPeriod = 1024;
const uint32_t kOptionsFastPeriod = 256;
const uint32_t kOptionsPulsePeriod = 2048;
const uint32_t kOptionsPreviewTime = 2048;
const uint32_t kSelfPlayHoldTime = 1500;
extern const OptionPage kOptionPages[kNumOptionPages];

// [A]+DRY/WET: fully CCW is stock; hysteresis keeps it from flickering.
StereoWidth StereoWidthFromPot(float pot, StereoWidth current);

enum PulsePattern {
  PULSE_STEADY, PULSE_SLOW, PULSE_FAST, PULSE_DOUBLE
};

struct OptionValue {
  OptionSetting setting;
  int value;
  Leds::Color color;
  PulsePattern pulses;
};
const size_t kNumOptionValues = 32;
// Shared by the MEDIA renderer, host export and documentation check.
extern const OptionValue kOptionValues[kNumOptionValues];

class Options {
 public:
  enum Stage {
    STAGE_INACTIVE,
    STAGE_WAIT_RELEASE,
    STAGE_EDITING,
    STAGE_DRAIN,
    STAGE_PENDING
  };

  Options() { }
  ~Options() { }

  void Init();
  void Configure(const Profile& profile);

  void Poll(uint32_t now, bool assign, bool other, bool all_up,
      bool just_pressed, bool normal, bool pot_touched, const float* pots,
      bool seed, bool seed_pressed, const Settings& settings);

  int Value(OptionSetting setting) const;

  const OptionValue& indication() const;

  inline bool active() const {
    return stage_ == STAGE_WAIT_RELEASE || stage_ == STAGE_EDITING;
  }

  inline Stage stage() const { return stage_; }

  inline int page() const { return page_; }

  inline bool any_page() const { return any_page_; }

  inline OutputLimiterMode limiter_preview() const { return limiter_preview_; }

  inline bool spectral_preview() const { return spectral_preview_; }

  inline bool manual_start_preview() const { return manual_start_preview_; }

  inline bool intervals_preview() const { return intervals_preview_; }

  inline bool clouds_preview() const { return clouds_preview_; }

  inline bool plaits_preview() const { return plaits_preview_; }

  inline DelayMode delay_preview() const { return delay_preview_; }

  inline PitchScale pitch_preview() const { return pitch_preview_; }

  inline bool evolving_preview() const { return evolving_preview_; }

  inline AttackMode attack_preview() const { return attack_preview_; }

  // The knob whose stored value [A] shows after a page opens, or -1.
  inline int preview_knob() const { return preview_knob_; }

  // Blinks start with the setting they show.
  inline uint32_t indication_time() const { return indication_time_; }

  inline bool seed_allowed() const {
    return stage_ == STAGE_EDITING && PageHasPitch() &&
        pitch_preview_ >= PITCH_SCALE_FIFTHS;
  }

  inline bool commit_requested() const { return commit_; }

  inline void ClearCommit() {
    dirty_ = false;
    commit_ = false;
  }

 private:
  bool PageHasPitch() const;
  bool PageEmpty(int page) const;
  OptionPage pages_[kNumOptionPages];
  bool any_page_;

  void ApplySeedTaps();

  void RequestCommit(uint32_t now);

  void ResetPots(const float* pots);

  void StartPreview(uint32_t now, int knob);

  void Select(uint32_t now, OptionSetting setting);

  void UpdateZone(float pot, int* zone);

  void Finish();

  // SysTick publishes transitions and a stable batch to main.
  volatile Stage stage_;
  volatile bool commit_;
  volatile bool dirty_;
  bool armed_;
  bool eligible_;
  bool holding_;
  uint32_t all_up_time_;
  uint32_t press_time_;
  uint32_t last_action_;
  int page_;
  OptionSetting selected_;
  int preview_knob_;
  uint32_t preview_time_;
  uint32_t indication_time_;
  float origin_[MIX_PARAMETER_LAST];
  float previous_[MIX_PARAMETER_LAST];
  bool qualified_[MIX_PARAMETER_LAST];
  int zone_[MIX_PARAMETER_LAST];
  OutputLimiterMode limiter_preview_;
  bool spectral_preview_;
  bool manual_start_preview_;
  bool intervals_preview_;
  bool clouds_preview_;
  bool plaits_preview_;
  DelayMode delay_preview_;
  PitchScale pitch_preview_;
  bool evolving_preview_;
  AttackMode attack_preview_;
  PitchScale scale_;
  bool seed_held_;
  uint32_t seed_time_;
  uint32_t seed_taps_;
  uint32_t seed_action_time_;

  DISALLOW_COPY_AND_ASSIGN(Options);
};

}  // namespace beads

#endif  // BEADS_OPTIONS_H_
