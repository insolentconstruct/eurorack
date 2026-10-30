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
// Guarded options page transitions.

#include "beads/options.h"

#include "beads/settings.h"

namespace beads {

const OptionPage kOptionPages[kNumOptionPages] = {
  { 0, { OPTION_PITCH, OPTION_INTERVALS, OPTION_ATTACKS } },
  { 1, { OPTION_DELAY, OPTION_REVERB, OPTION_FROZEN } },
  { 2, { OPTION_PROCESSOR, OPTION_SYNTH, OPTION_SYNTH_START } },
  { MIX_PARAMETER_LAST, { OPTION_RESERVED, OPTION_RESERVED, OPTION_LIMITER } }
};
const OptionValue kOptionValues[] = {
  { OPTION_RESERVED, 0,
    { 0, 0, 0 }, PULSE_STEADY },
  { OPTION_PITCH, 0,
    { 0, 255, 0 }, PULSE_STEADY },
  { OPTION_PITCH, 1,
    { 255, 255, 0 }, PULSE_STEADY },
  { OPTION_PITCH, 2,
    { 255, 0, 0 }, PULSE_STEADY },
  { OPTION_PITCH, 3,
    { 0, 255, 0 }, PULSE_SLOW },
  { OPTION_PITCH, 4,
    { 255, 255, 0 }, PULSE_SLOW },
  { OPTION_PITCH, 5,
    { 255, 0, 0 }, PULSE_SLOW },
  { OPTION_PITCH, 6,
    { 0, 255, 0 }, PULSE_FAST },
  { OPTION_PITCH, 7,
    { 255, 255, 0 }, PULSE_FAST },
  { OPTION_PITCH, 8,
    { 255, 0, 0 }, PULSE_FAST },
  { OPTION_PITCH, 9,
    { 255, 255, 0 }, PULSE_DOUBLE },
  { OPTION_REVERB, 0,
    { 0, 255, 0 }, PULSE_STEADY },
  { OPTION_REVERB, 1,
    { 255, 255, 0 }, PULSE_STEADY },
  { OPTION_DELAY, 0,
    { 0, 255, 0 }, PULSE_STEADY },
  { OPTION_DELAY, 1,
    { 255, 255, 0 }, PULSE_STEADY },
  { OPTION_DELAY, 2,
    { 255, 0, 0 }, PULSE_STEADY },
  { OPTION_PROCESSOR, 0,
    { 0, 255, 0 }, PULSE_STEADY },
  { OPTION_PROCESSOR, 1,
    { 255, 255, 0 }, PULSE_STEADY },
  { OPTION_LIMITER, 0,
    { 0, 255, 0 }, PULSE_STEADY },
  { OPTION_LIMITER, 1,
    { 255, 255, 0 }, PULSE_STEADY },
  { OPTION_LIMITER, 2,
    { 255, 0, 0 }, PULSE_STEADY },
  { OPTION_FROZEN, 0,
    { 0, 255, 0 }, PULSE_STEADY },
  { OPTION_FROZEN, 1,
    { 255, 255, 0 }, PULSE_STEADY },
  { OPTION_SYNTH, 0,
    { 0, 255, 0 }, PULSE_STEADY },
  { OPTION_SYNTH, 1,
    { 255, 255, 0 }, PULSE_STEADY },
  { OPTION_ATTACKS, 0,
    { 0, 255, 0 }, PULSE_STEADY },
  { OPTION_ATTACKS, 1,
    { 255, 255, 0 }, PULSE_STEADY },
  { OPTION_ATTACKS, 2,
    { 255, 0, 0 }, PULSE_STEADY },
  { OPTION_SYNTH_START, 0,
    { 0, 255, 0 }, PULSE_STEADY },
  { OPTION_SYNTH_START, 1,
    { 255, 255, 0 }, PULSE_STEADY },
  { OPTION_INTERVALS, 0,
    { 0, 255, 0 }, PULSE_STEADY },
  { OPTION_INTERVALS, 1,
    { 255, 255, 0 }, PULSE_STEADY },
  { OPTION_BRIGHTNESS, 0, { 0, 255, 0 }, PULSE_STEADY },
  { OPTION_BRIGHTNESS, 1, { 255, 255, 0 }, PULSE_STEADY },
  { OPTION_BRIGHTNESS, 2, { 255, 0, 0 }, PULSE_STEADY },
};

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
  selected_ = OPTION_PITCH;
  preview_knob_ = -1;
  preview_time_ = indication_time_ = 0;
  limiter_preview_ = OUTPUT_LIMITER_STOCK;
  clouds_preview_ = evolving_preview_ = plaits_preview_ = false;
  spectral_preview_ = manual_start_preview_ = intervals_preview_ = false;
  attack_preview_ = ATTACK_MODE_OFF;
  pitch_preview_ = PITCH_SCALE_OFF;
  scale_ = PITCH_SCALE_FIFTHS;
  delay_preview_ = DELAY_MODE_AUTO;
  seed_held_ = false;
  seed_time_ = 0;
  seed_taps_ = 0;
  seed_action_time_ = 0;
  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    origin_[i] = previous_[i] = 0.0f;
    qualified_[i] = false;
    zone_[i] = 0;
  }
}

void Options::Configure(const Profile& profile) {
  any_page_ = false;
  for (int i = 0; i < OPTION_LAST; ++i) {
    profile_preview_[i] = 0;
  }
  for (int page = 0; page < kNumOptionPages; ++page) {
    for (int knob = 0; knob < MIX_PARAMETER_LAST; ++knob) {
      pages_[page].knobs[knob] = OptionSetting(
          profile.Page(1 + page * MIX_PARAMETER_LAST + knob));
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
        StartPreview(now, 0);
      }
      return;
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
      StartPreview(now, 0);
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
      const OptionSetting setting = pages_[page_].knobs[i];
      if (setting == OPTION_RESERVED) {
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
      switch (setting) {
        case OPTION_BRIGHTNESS: {
          int value = profile_preview_[setting];
          while (value < 2 && pot > float(value + 1) / 3.0f + 0.025f) {
            ++value;
          }
          while (value > 0 && pot < float(value) / 3.0f - 0.025f) {
            --value;
          }
          profile_preview_[setting] = value;
        } break;
        case OPTION_PITCH:
          pitch_preview_ = zone_[i] == 2 ? scale_ : PitchScale(zone_[i]);
          break;
        case OPTION_REVERB:
          if (pot < kOptionsHalfLow) {
            clouds_preview_ = false;
          }

          if (pot > kOptionsHalfHigh) {
            clouds_preview_ = true;
          }
          break;
        case OPTION_DELAY:
          delay_preview_ = DelayMode(zone_[i]);
          break;
        case OPTION_PROCESSOR:
          if (pot < kOptionsHalfLow) {
            spectral_preview_ = false;
          }

          if (pot > kOptionsHalfHigh) {
            spectral_preview_ = true;
          }
          break;
        case OPTION_INTERVALS:
          if (pot < kOptionsHalfLow) {
            intervals_preview_ = false;
          }

          if (pot > kOptionsHalfHigh) {
            intervals_preview_ = true;
          }
          break;
        case OPTION_SYNTH_START:
          if (pot < kOptionsHalfLow) {
            manual_start_preview_ = false;
          }

          if (pot > kOptionsHalfHigh) {
            manual_start_preview_ = true;
          }
          break;
        case OPTION_LIMITER:
          limiter_preview_ = OutputLimiterMode(zone_[i]);
          break;
        case OPTION_ATTACKS:
          attack_preview_ = AttackMode(zone_[i]);
          break;
        case OPTION_FROZEN:
          if (pot < kOptionsHalfLow) {
            evolving_preview_ = false;
          }

          if (pot > kOptionsHalfHigh) {
            evolving_preview_ = true;
          }
          break;
        case OPTION_SYNTH:
          if (pot < kOptionsHalfLow) {
            plaits_preview_ = false;
          }

          if (pot > kOptionsHalfHigh) {
            plaits_preview_ = true;
          }
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
    for (int i = FEATURE_VERSION; i < OPTION_LAST; ++i) {
      profile_preview_[i] = settings.FeatureValue(ProfileFeature(i));
    }
    limiter_preview_ = settings.output_limiter_mode();
    spectral_preview_ = settings.spectral();
    manual_start_preview_ = settings.self_play_on_request();
    intervals_preview_ = settings.random_intervals();
    clouds_preview_ = settings.clouds_style();
    plaits_preview_ = settings.plaits_synth();
    pitch_preview_ = settings.pitch_scale();
    scale_ = settings.pitch_scale() >= PITCH_SCALE_FIFTHS
        ? settings.pitch_scale() : PITCH_SCALE_FIFTHS;
    delay_preview_ = settings.delay_mode();
    evolving_preview_ = settings.evolving_feedback();
    attack_preview_ = settings.attack_mode();
    ResetPots(pots);
    eligible_ = false;
    stage_ = STAGE_WAIT_RELEASE;
  }
}

int Options::Value(OptionSetting setting) const {
  if (setting >= OPTION_BRIGHTNESS && setting < OPTION_LAST) {
    return profile_preview_[setting];
  }
  switch (setting) {
    case OPTION_PITCH:
      return pitch_preview_;
    case OPTION_REVERB:
      return clouds_preview_;
    case OPTION_DELAY:
      return delay_preview_;
    case OPTION_PROCESSOR:
      return spectral_preview_;
    case OPTION_SYNTH_START:
      return manual_start_preview_;
    case OPTION_INTERVALS:
      return intervals_preview_;
    case OPTION_LIMITER:
      return limiter_preview_;
    case OPTION_FROZEN:
      return evolving_preview_;
    case OPTION_SYNTH:
      return plaits_preview_;
    case OPTION_ATTACKS:
      return attack_preview_;
    default:
      return 0;
  }
}

const OptionValue& Options::indication() const {
  const int value = Value(selected_);
  for (size_t i = 0; i < kNumOptionValues; ++i) {
    if (kOptionValues[i].setting == selected_ &&
        kOptionValues[i].value == value) {
      return kOptionValues[i];
    }
  }

  return kOptionValues[0];
}

void Options::ApplySeedTaps() {
  if (commit_) {
    return;
  }
  while (seed_taps_) {
    --seed_taps_;
    scale_ = scale_ == PITCH_SCALE_SHRI ? PITCH_SCALE_FIFTHS
        : PitchScale(scale_ + 1);
    pitch_preview_ = scale_;
    preview_knob_ = -1;
    selected_ = OPTION_PITCH;
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
    origin_[i] = previous_[i] = pots[i];
    qualified_[i] = false;
  }

  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    if (pages_[page_].knobs[i] != OPTION_RESERVED) {
      selected_ = pages_[page_].knobs[i];
      break;
    }
  }

  preview_knob_ = -1;
  seed_held_ = false;
}

bool Options::PageEmpty(int page) const {
  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    if (pages_[page].knobs[i] != OPTION_RESERVED) {
      return false;
    }
  }
  return true;
}

bool Options::PageHasPitch() const {
  for (int i = 0; i < MIX_PARAMETER_LAST; ++i) {
    if (pages_[page_].knobs[i] == OPTION_PITCH) {
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
         pages_[page_].knobs[knob] == OPTION_RESERVED) {
    ++knob;
  }

  if (knob == MIX_PARAMETER_LAST) {
    // Back to the first setting, as before the preview.
    preview_knob_ = -1;
    knob = 0;
    while (pages_[page_].knobs[knob] == OPTION_RESERVED) {
      ++knob;
    }
  } else {
    preview_knob_ = knob;
    preview_time_ = now;
  }

  selected_ = pages_[page_].knobs[knob];
  indication_time_ = now;
}

void Options::Select(uint32_t now, OptionSetting setting) {
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
