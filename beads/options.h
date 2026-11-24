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
// Guarded options page; tables come from resources/features.json.

#ifndef BEADS_OPTIONS_H_
#define BEADS_OPTIONS_H_

#include <cmath>

#include "stmlib/stmlib.h"

#include "beads/drivers/leds.h"
#include "beads/dsp/attack_detector.h"
#include "beads/dsp/limiter.h"
#include "beads/dsp/pitch_quantizer.h"
#include "beads/dsp/playback_options.h"
#include "beads/profile.h"

namespace beads {

class Settings;

enum { MIX_PARAMETER_LAST = 3 };
const int kNumOptionPages = 7;

// Value counts of the page features, checked against their enums.
enum PageValues {
  PAGE_VALUES_PITCH = 16,
  PAGE_VALUES_REVERB = 2,
  PAGE_VALUES_DELAY = 3,
  PAGE_VALUES_PROCESSOR = 3,
  PAGE_VALUES_LIMITER = 3,
  PAGE_VALUES_FROZEN = 2,
  PAGE_VALUES_SYNTH = 3,
  PAGE_VALUES_ATTACKS = 3,
  PAGE_VALUES_SYNTH_START = 2,
  PAGE_VALUES_INTERVALS = 3,
  PAGE_VALUES_BRIGHTNESS = 3,
  PAGE_VALUES_SEED_LATCH = 4,
  PAGE_VALUES_TIME_SCAN = 2,
  PAGE_VALUES_RESONATOR = 4,
  PAGE_VALUES_RESONATOR_MATERIAL = 8,
  PAGE_VALUES_RESONATOR_RING = 4,
  PAGE_VALUES_GAIN_CEILING = 4,
};

// How a knob picks a page feature's value.
enum PageControl {
  PAGE_CONTROL_NONE,
  PAGE_CONTROL_HALVES,
  PAGE_CONTROL_ZONES,
  PAGE_CONTROL_STEPS,
  PAGE_CONTROL_SCALE
};

// Pages 1-3 light one mix LED, pages 4-7 two or all three of them.
struct OptionPage {
  uint8_t leds;  // Bit 0 FEEDBACK, bit 1 DRY/WET, bit 2 REVERB.
  ProfileFeature knobs[MIX_PARAMETER_LAST];  // FEEDBACK, DRY/WET, REVERB.
};

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
const float kOptionsProfileZoneBand = 0.025f;
const uint32_t kOptionsSettleTime = 1000;
const uint32_t kOptionsFlashTime = 100;
const uint32_t kOptionsSlowPeriod = 1024;
const uint32_t kOptionsFastPeriod = 256;
const uint32_t kOptionsPulsePeriod = 2048;
const uint32_t kOptionsPreviewTime = 2048;
const uint32_t kSelfPlayHoldTime = 1500;
// Larger than pot jitter; slow turns count once they add up to it.
const float kOptionsIdleMovement = 0.02f;
const int kNumIdleTimeouts = 5;
extern const uint32_t kOptionsIdleTimeouts[kNumIdleTimeouts];
extern const OptionPage kOptionPages[kNumOptionPages];
const int kNumPageFeatures = 17;
extern const ProfileFeature kPageFeatures[kNumPageFeatures];
extern const uint8_t kPageControls[FEATURE_LAST];

// [A]+DRY/WET: fully CCW is stock; hysteresis keeps it from flickering.
StereoWidth StereoWidthFromPot(float pot, StereoWidth current);

enum PulsePattern {
  PULSE_STEADY, PULSE_SLOW, PULSE_FAST, PULSE_DOUBLE
};

struct OptionValue {
  ProfileFeature setting;
  uint8_t value;
  Leds::Color color;
  PulsePattern pulses;
};
const int kNumOptionColors = 3;
extern const Leds::Color kOptionColors[kNumOptionColors];
const size_t kNumOptionValues = 18;
// The indications other than the shared colours, then none.
extern const OptionValue kOptionValues[kNumOptionValues];
// A value's colour and pattern on its options page.
OptionValue FindOptionValue(ProfileFeature setting, int value);

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

  // The synths Synth offers, as SynthMask bits.
  inline void set_playable_synths(uint8_t playable) {
    playable_synths_ = playable;
  }

  void Poll(uint32_t now, bool assign, bool other, bool all_up,
      bool just_pressed, bool normal, bool pot_touched, const float* pots,
      bool seed, bool seed_pressed, const Settings& settings);

  inline int Value(ProfileFeature setting) const { return preview_[setting]; }

  const OptionValue& indication() const;

  inline bool active() const {
    return stage_ == STAGE_WAIT_RELEASE || stage_ == STAGE_EDITING;
  }

  inline Stage stage() const { return stage_; }

  inline int page() const { return page_; }

  inline bool any_page() const { return any_page_; }

  // The knob whose stored value [A] shows after a page opens, or -1.
  inline int preview_knob() const { return preview_knob_; }

  // Blinks start with the setting they show.
  inline uint32_t indication_time() const { return indication_time_; }

  inline bool seed_allowed() const {
    return stage_ == STAGE_EDITING && PageHasPitch() &&
        preview_[FEATURE_PITCH] >= PITCH_SCALE_FIFTHS;
  }

  inline bool commit_requested() const { return commit_; }

  inline void ClearCommit() {
    dirty_ = false;
    commit_ = false;
  }

 private:
  bool PageHasPitch() const;
  bool PageEmpty(int page) const;
  void ApplySeedTaps();
  void RequestCommit(uint32_t now);
  void ResetPots(const float* pots);
  void StartPreview(uint32_t now, int knob);
  void Select(uint32_t now, ProfileFeature setting);
  void UpdateZone(float pot, int* zone);
  uint8_t PlayableSynth(float pot, int zone, uint8_t value) const;
  void Finish();

  OptionPage pages_[kNumOptionPages];
  bool any_page_;
  // The values on the pages, committed together.
  uint8_t preview_[FEATURE_LAST];

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
  ProfileFeature selected_;
  int preview_knob_;
  int first_preview_knob_;
  bool first_page_;
  uint32_t preview_time_;
  uint32_t indication_time_;
  uint32_t idle_timeout_;
  uint32_t idle_time_;
  float idle_pot_[MIX_PARAMETER_LAST];
  float origin_[MIX_PARAMETER_LAST];
  float previous_[MIX_PARAMETER_LAST];
  bool qualified_[MIX_PARAMETER_LAST];
  int zone_[MIX_PARAMETER_LAST];
  PitchScale scale_;
  uint8_t playable_synths_;
  const Profile* profile_;
  mutable OptionValue indication_;
  bool seed_held_;
  uint32_t seed_time_;
  uint32_t seed_taps_;
  uint32_t seed_action_time_;

  DISALLOW_COPY_AND_ASSIGN(Options);
};

}  // namespace beads

#endif  // BEADS_OPTIONS_H_
