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
// Guarded options page transitions; tables come from resources/features.json.

#include "beads/options.h"

#include "beads/settings.h"

namespace beads {

// Previews are these enums' values; slots 1-21 are the pages' knobs.
STATIC_ASSERT(PAGE_VALUES_PITCH == kMaxPitchScaleId + 1, pitch_values);
STATIC_ASSERT(PAGE_VALUES_DELAY == DELAY_MODE_WIDE_DETENT + 1, delay_values);
STATIC_ASSERT(PAGE_VALUES_LIMITER == OUTPUT_LIMITER_OFF + 1, limiter_values);
STATIC_ASSERT(
    PAGE_VALUES_ATTACKS == ATTACK_MODE_SNAP_AND_TRIGGER + 1, attack_values);
STATIC_ASSERT(
    PAGE_VALUES_INTERVALS == int(RANDOM_INTERVALS_LAST), interval_values);
STATIC_ASSERT(
    kNumPageSlots == 1 + kNumOptionPages * MIX_PARAMETER_LAST, page_slots);

const OptionPage kOptionPages[kNumOptionPages] = {
  { 1, { FEATURE_PITCH, FEATURE_INTERVALS, FEATURE_ATTACKS } },
  { 2, { FEATURE_FROZEN, FEATURE_DELAY, FEATURE_REVERB } },
  { 4, { FEATURE_PROCESSOR, FEATURE_SYNTH, FEATURE_SYNTH_START } },
  { 3, { FEATURE_BRIGHTNESS, FEATURE_SEED_LATCH, FEATURE_LIMITER } },
  { 5, { FEATURE_NONE, FEATURE_NONE, FEATURE_NONE } },
  { 6, { FEATURE_NONE, FEATURE_NONE, FEATURE_NONE } },
  { 7, { FEATURE_NONE, FEATURE_NONE, FEATURE_NONE } }
};
const ProfileFeature kPageFeatures[kNumPageFeatures] = {
  FEATURE_PITCH,
  FEATURE_REVERB,
  FEATURE_DELAY,
  FEATURE_PROCESSOR,
  FEATURE_LIMITER,
  FEATURE_FROZEN,
  FEATURE_SYNTH,
  FEATURE_ATTACKS,
  FEATURE_SYNTH_START,
  FEATURE_INTERVALS,
  FEATURE_BRIGHTNESS,
  FEATURE_SEED_LATCH,
  FEATURE_TIME_SCAN,
};
const uint8_t kPageControls[FEATURE_LAST] = {
  PAGE_CONTROL_NONE,  // None.
  PAGE_CONTROL_SCALE,  // Pitch.
  PAGE_CONTROL_HALVES,  // Reverb.
  PAGE_CONTROL_ZONES,  // Delay.
  PAGE_CONTROL_HALVES,  // Processor.
  PAGE_CONTROL_ZONES,  // Limiter.
  PAGE_CONTROL_HALVES,  // Frozen.
  PAGE_CONTROL_HALVES,  // Synth.
  PAGE_CONTROL_ZONES,  // Attacks.
  PAGE_CONTROL_HALVES,  // Synth start.
  PAGE_CONTROL_ZONES,  // Intervals.
  PAGE_CONTROL_NONE,  // Width.
  PAGE_CONTROL_NONE,  // Diffusion.
  PAGE_CONTROL_NONE,  // Monophonic.
  PAGE_CONTROL_NONE,  // Trim.
  PAGE_CONTROL_NONE,  // Version.
  PAGE_CONTROL_NONE,  // Attenuation.
  PAGE_CONTROL_NONE,  // Delay cue.
  PAGE_CONTROL_STEPS,  // Brightness.
  PAGE_CONTROL_NONE,  // Live assign.
  PAGE_CONTROL_NONE,  // Save freeze.
  PAGE_CONTROL_STEPS,  // Seed latch.
  PAGE_CONTROL_NONE,  // Pitch notch.
  PAGE_CONTROL_HALVES,  // Time scan.
  PAGE_CONTROL_NONE,  // Synth tail.
  PAGE_CONTROL_NONE,  // Idle timeout.
  PAGE_CONTROL_NONE,  // Seed latch fade.
  PAGE_CONTROL_NONE,  // Manual gain.
  PAGE_CONTROL_NONE,  // Attack sensitivity.
  PAGE_CONTROL_NONE,  // Seed start.
  PAGE_CONTROL_NONE,  // Pitch law.
  PAGE_CONTROL_NONE,  // Plaits bank.
  PAGE_CONTROL_NONE,  // Mix law.
  PAGE_CONTROL_NONE,  // Reverb input.
  PAGE_CONTROL_NONE,  // Grain stealing.
  PAGE_CONTROL_NONE,  // Unity feedback.
  PAGE_CONTROL_NONE,  // Delay crossfade.
  PAGE_CONTROL_NONE,  // Page preview.
  PAGE_CONTROL_NONE,  // First page.
  PAGE_CONTROL_NONE,  // Start quality.
  PAGE_CONTROL_NONE,  // Gain knob.
  PAGE_CONTROL_NONE,  // Resonator.
};
const Leds::Color kOptionColors[kNumOptionColors] = {
  { 0, 255, 0 },
  { 255, 255, 0 },
  { 255, 0, 0 }
};
const OptionValue kOptionValues[] = {
  { FEATURE_NONE, 0, { 0, 0, 0 }, PULSE_STEADY },
  { FEATURE_PITCH, 2, { 255, 64, 32 }, PULSE_STEADY },
  { FEATURE_PITCH, 3, { 255, 160, 0 }, PULSE_STEADY },
  { FEATURE_PITCH, 4, { 32, 96, 255 }, PULSE_STEADY },
  { FEATURE_PITCH, 5, { 255, 160, 0 }, PULSE_SLOW },
  { FEATURE_PITCH, 6, { 32, 96, 255 }, PULSE_SLOW },
  { FEATURE_PITCH, 7, { 0, 220, 160 }, PULSE_STEADY },
  { FEATURE_PITCH, 8, { 160, 48, 255 }, PULSE_STEADY },
  { FEATURE_PITCH, 9, { 255, 32, 144 }, PULSE_DOUBLE },
  { FEATURE_SEED_LATCH, 3, { 0, 255, 0 }, PULSE_SLOW },
};

const uint32_t kOptionsIdleTimeouts[kNumIdleTimeouts] = {
  0, 5000, 10000, 20000, 30000
};

OptionValue FindOptionValue(ProfileFeature setting, int value) {
  for (size_t i = 1; i < kNumOptionValues; ++i) {
    if (kOptionValues[i].setting == setting &&
        kOptionValues[i].value == value) {
      return kOptionValues[i];
    }
  }
  OptionValue shared = kOptionValues[0];
  const FeatureDefinition& d = kFeatureDefinitions[setting];
  if (d.page && value < kNumOptionColors && value < d.values) {
    shared.setting = setting;
    shared.value = value;
    shared.color = kOptionColors[value];
  }
  return shared;
}

StereoWidth StereoWidthFromPot(float pot, StereoWidth current) {
  bool enabled = current != 0;
  if (pot < kWidthStockThreshold) {
    enabled = false;
  }

  if (pot > kWidthEnabledThreshold) {
    enabled = true;
  }

  float amount = (pot - kWidthEnabledThreshold) / kWidthRange;
  CONSTRAIN(amount, 0.0f, 1.0f);
  return enabled ? 1 + int(amount * float(kStereoWidthMax - 1) + 0.5f) : 0;
}

void Options::Init() {
  stage_ = STAGE_INACTIVE;
  armed_ = eligible_ = holding_ = commit_ = dirty_ = false;
  all_up_time_ = press_time_ = last_action_ = 0;
  page_ = 0;
  any_page_ = true;
  for (int i = 0; i < kNumOptionPages; ++i) {
    pages_[i] = kOptionPages[i];
  }
  selected_ = FEATURE_PITCH;
  preview_knob_ = -1;
  first_preview_knob_ = 0;
  first_page_ = false;
  preview_time_ = indication_time_ = 0;
  idle_timeout_ = idle_time_ = 0;
  for (int i = 0; i < FEATURE_LAST; ++i) {
    preview_[i] = 0;
  }
  scale_ = PITCH_SCALE_FIFTHS;
  profile_ = NULL;
  seed_held_ = false;
  seed_time_ = 0;
  seed_taps_ = 0;
  seed_action_time_ = 0;
  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    idle_pot_[i] = origin_[i] = previous_[i] = 0.0f;
    qualified_[i] = false;
    zone_[i] = 0;
  }
}

void Options::Configure(const Profile& profile) {
  profile_ = &profile;
  const ProfileEntry& idle = profile.entry(FEATURE_IDLE_TIMEOUT);
  idle_timeout_ = idle.state == PROFILE_FIXED
      ? kOptionsIdleTimeouts[idle.value] : 0;
  // Starting past the last knob skips the walk through the stored values.
  const ProfileEntry& preview = profile.entry(FEATURE_PAGE_PREVIEW);
  first_preview_knob_ = preview.state == PROFILE_FIXED && preview.value
      ? MIX_PARAMETER_LAST : 0;
  const ProfileEntry& first_page = profile.entry(FEATURE_FIRST_PAGE);
  first_page_ = first_page.state == PROFILE_FIXED && first_page.value;
  any_page_ = false;
  for (int page = 0; page < kNumOptionPages; ++page) {
    for (int knob = 0; knob < MIX_PARAMETER_LAST; ++knob) {
      pages_[page].knobs[knob] = profile.Page(
          1 + page * MIX_PARAMETER_LAST + knob);
    }
    any_page_ |= !PageEmpty(page);
  }
  if (any_page_) {
    while (PageEmpty(page_)) {
      page_ = (page_ + 1) % kNumOptionPages;
    }
  }
}

void Options::Poll(uint32_t now, bool assign, bool other, bool all_up,
    bool just_pressed, bool normal, bool pot_touched, const float* pots,
    bool seed, bool seed_pressed, const Settings& settings) {
  // Release the exit gesture separately from the outstanding save batch.
  if (stage_ == STAGE_DRAIN || stage_ == STAGE_PENDING) {
    ApplySeedTaps();
    RequestCommit(now);
    if (all_up) {
      stage_ = STAGE_PENDING;
    }

    if (stage_ == STAGE_PENDING && !dirty_ && !commit_) {
      stage_ = STAGE_INACTIVE;
      all_up_time_ = now;
      armed_ = false;
    }
    return;
  }

  if (stage_ != STAGE_INACTIVE) {
    if (other || !normal) {
      Finish();
      RequestCommit(now);
      return;
    }

    if (stage_ == STAGE_WAIT_RELEASE) {
      if (all_up) {
        stage_ = STAGE_EDITING;
        holding_ = false;
        idle_time_ = now;
        StartPreview(now, first_preview_knob_);
      }
      return;
    }

    // Held buttons and knob turns restart the idle timeout.
    if (idle_timeout_) {
      if (!all_up) {
        idle_time_ = now;
      }
      for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
        // As Clouds' blend knob: touched once 2% from the last touch.
        if (pages_[page_].knobs[i] != FEATURE_NONE &&
            fabsf(pots[i] - idle_pot_[i]) > kOptionsIdleMovement) {
          idle_pot_[i] = pots[i];
          idle_time_ = now;
        }
      }
      if (uint32_t(now - idle_time_) >= idle_timeout_) {
        Finish();
        RequestCommit(now);
        return;
      }
    }

    if (just_pressed) {
      holding_ = true;
      press_time_ = now;
    }

    if (holding_ && assign &&
        uint32_t(now - press_time_) >= kOptionsHoldTime) {
      Finish();
      RequestCommit(now);
      return;
    }

    if (holding_ && all_up) {
      holding_ = false;
      do {
        page_ = (page_ + 1) % kNumOptionPages;
      } while (PageEmpty(page_));
      ResetPots(pots);
      StartPreview(now, first_preview_knob_);
    }

    // Show each stored value in turn until a knob takes over.
    if (preview_knob_ >= 0 &&
        uint32_t(now - preview_time_) >= kOptionsPreviewTime) {
      StartPreview(now, preview_knob_ + 1);
    }

    // Capture real edges even while main owns the published preview.
    if (seed_allowed() && !assign) {
      if (seed_pressed) {
        seed_time_ = now;
        seed_held_ = true;
      }

      if (!seed && seed_held_) {
        if (uint32_t(now - seed_time_) < kOptionsTapTime) {
          ++seed_taps_;
          seed_action_time_ = now;
        }

        seed_held_ = false;
      }
    } else {
      seed_held_ = false;
    }

    // Main consumes a stable batch. Navigation and exit still work while
    // flash is busy; only edits wait for ClearCommit.
    if (commit_) {
      return;
    }

    ApplySeedTaps();
    for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
      const ProfileFeature setting = pages_[page_].knobs[i];
      if (setting == FEATURE_NONE) {
        continue;
      }

      const float pot = pots[i];
      if (!qualified_[i] &&
          fabsf(pot - origin_[i]) > kOptionsPickupThreshold) {
        qualified_[i] = true;
        zone_[i] = pot < kOptionsZoneLow ? 0 : pot < kOptionsZoneHigh ? 1 : 2;
        preview_knob_ = -1;
        Select(now, setting);
      }

      if (!qualified_[i]) {
        continue;
      }

      if (fabsf(pot - previous_[i]) > kOptionsMovementThreshold) {
        previous_[i] = pot;
        Select(now, setting);
        last_action_ = now;
      }

      const int before = Value(setting);
      UpdateZone(pot, &zone_[i]);
      uint8_t* value = &preview_[setting];
      switch (kPageControls[setting]) {
        case PAGE_CONTROL_HALVES:
          if (pot < kOptionsHalfLow) {
            *value = 0;
          }

          if (pot > kOptionsHalfHigh) {
            *value = 1;
          }
          break;
        case PAGE_CONTROL_ZONES:
          *value = zone_[i];
          break;
        case PAGE_CONTROL_STEPS:
          {
            // Equal steps, each held a little past its edge.
            const int last = kFeatureDefinitions[setting].values - 1;
            const float steps = float(last + 1);
            int step = *value;
            while (step < last &&
                pot > float(step + 1) / steps + kOptionsProfileZoneBand) {
              ++step;
            }
            while (step > 0 &&
                pot < float(step) / steps - kOptionsProfileZoneBand) {
              --step;
            }
            *value = step;
          }
          break;
        case PAGE_CONTROL_SCALE:
          *value = zone_[i] == 2 ? scale_ : PitchScale(zone_[i]);
          break;
        default:
          break;
      }

      if (before != Value(setting)) {
        dirty_ = true;
        last_action_ = now;
      }
    }

    RequestCommit(now);
    return;
  }

  if (all_up) {
    if (uint32_t(now - all_up_time_) >= kOptionsArmTime) {
      armed_ = true;
    }

    eligible_ = false;
  } else {
    all_up_time_ = now;
  }

  if (just_pressed) {
    eligible_ = any_page_ && armed_ && normal && !other && !pot_touched;
    armed_ = false;
    press_time_ = now;
  }

  if (other || pot_touched || !normal) {
    eligible_ = false;
  }

  if (eligible_ && assign &&
      uint32_t(now - press_time_) >= kOptionsHoldTime) {
    for (int i = 0; i < kNumPageFeatures; ++i) {
      const ProfileFeature f = kPageFeatures[i];
      preview_[f] = settings.FeatureValue(f);
    }
    const PitchScale pitch = PitchScale(preview_[FEATURE_PITCH]);
    scale_ = pitch >= PITCH_SCALE_FIFTHS
        ? pitch : settings.profile().FirstScale();
    if (first_page_) {
      page_ = 0;
      while (PageEmpty(page_)) {
        ++page_;
      }
    }
    ResetPots(pots);
    eligible_ = false;
    stage_ = STAGE_WAIT_RELEASE;
  }
}

const OptionValue& Options::indication() const {
  const int value = Value(selected_);
  if (selected_ == FEATURE_PITCH && profile_) {
    const PitchScaleDefinition* scale = profile_->FindScale(value);
    if (scale) {
      indication_.setting = FEATURE_PITCH;
      indication_.value = value;
      indication_.color.r = scale->color[0];
      indication_.color.g = scale->color[1];
      indication_.color.b = scale->color[2];
      indication_.pulses = PulsePattern(scale->pulses);
      return indication_;
    }
  }
  indication_ = FindOptionValue(selected_, value);
  return indication_;
}

void Options::ApplySeedTaps() {
  if (commit_) {
    return;
  }
  while (seed_taps_) {
    --seed_taps_;
    scale_ = profile_ ? profile_->NextScale(scale_)
        : scale_ == PITCH_SCALE_SHRI ? PITCH_SCALE_FIFTHS
            : PitchScale(scale_ + 1);
    preview_[FEATURE_PITCH] = scale_;
    preview_knob_ = -1;
    selected_ = FEATURE_PITCH;
    indication_time_ = seed_action_time_;
    dirty_ = true;
    last_action_ = seed_action_time_;
  }
}

void Options::RequestCommit(uint32_t now) {
  if (!commit_ && dirty_ &&
      uint32_t(now - last_action_) >= kOptionsSettleTime) {
    commit_ = true;
  }
}

void Options::ResetPots(const float* pots) {
  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    idle_pot_[i] = origin_[i] = previous_[i] = pots[i];
    qualified_[i] = false;
  }

  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    if (pages_[page_].knobs[i] != FEATURE_NONE) {
      selected_ = pages_[page_].knobs[i];
      break;
    }
  }

  preview_knob_ = -1;
  seed_held_ = false;
}

bool Options::PageEmpty(int page) const {
  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    if (pages_[page].knobs[i] != FEATURE_NONE) {
      return false;
    }
  }
  return true;
}

bool Options::PageHasPitch() const {
  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    if (pages_[page_].knobs[i] == FEATURE_PITCH) {
      return true;
    }
  }
  return false;
}

void Options::StartPreview(uint32_t now, int knob) {
  if (PageEmpty(page_)) {
    preview_knob_ = -1;
    return;
  }
  while (knob < MIX_PARAMETER_LAST &&
         pages_[page_].knobs[knob] == FEATURE_NONE) {
    ++knob;
  }

  if (knob == MIX_PARAMETER_LAST) {
    // Back to the first setting, as before the preview.
    preview_knob_ = -1;
    knob = 0;
    while (pages_[page_].knobs[knob] == FEATURE_NONE) {
      ++knob;
    }
  } else {
    preview_knob_ = knob;
    preview_time_ = now;
  }

  selected_ = pages_[page_].knobs[knob];
  indication_time_ = now;
}

void Options::Select(uint32_t now, ProfileFeature setting) {
  if (setting != selected_) {
    selected_ = setting;
    indication_time_ = now;
  }
}

void Options::UpdateZone(float pot, int* zone) {
  if (pot < kOptionsZoneLow - kOptionsZoneBand) {
    *zone = 0;
  } else if (pot > kOptionsZoneHigh + kOptionsZoneBand) {
    *zone = 2;
  } else if ((*zone == 0 && pot > kOptionsZoneLow + kOptionsZoneBand) ||
             (*zone == 2 && pot < kOptionsZoneHigh - kOptionsZoneBand)) {
    *zone = 1;
  }
}

void Options::Finish() {
  stage_ = STAGE_DRAIN;
  holding_ = false;
  seed_held_ = false;
}

}  // namespace beads
