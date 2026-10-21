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
  { 2, { OPTION_PITCH, OPTION_WIDTH, OPTION_REVERB } },
  { 0, { OPTION_ATTACKS, OPTION_DELAY, OPTION_FROZEN } },
  { 1, { OPTION_ENGINE, OPTION_LIMITER, OPTION_SYNTH } }
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
  { OPTION_WIDTH, 0,
    { 0, 255, 0 }, PULSE_STEADY },
  { OPTION_WIDTH, 1,
    { 255, 255, 0 }, PULSE_SLOW },
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
  { OPTION_ENGINE, 0,
    { 0, 255, 0 }, PULSE_STEADY },
  { OPTION_ENGINE, 1,
    { 255, 255, 0 }, PULSE_STEADY },
  { OPTION_ENGINE, 2,
    { 255, 0, 0 }, PULSE_STEADY },
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
    { 255, 0, 0 }, PULSE_STEADY }
};

void Options::Init() {
  stage_ = STAGE_INACTIVE;
  armed_ = eligible_ = holding_ = commit_ = dirty_ = false;
  all_up_time_ = press_time_ = last_action_ = 0;
  page_ = 0;
  selected_ = OPTION_PITCH;
  limiter_preview_ = OUTPUT_LIMITER_STOCK;
  clouds_preview_ = evolving_preview_ = plaits_preview_ = false;
  engine_preview_ = ENGINE_GRANULAR;
  attack_preview_ = ATTACK_MODE_OFF;
  pitch_preview_ = PITCH_SCALE_OFF;
  scale_ = PITCH_SCALE_FIFTHS;
  width_preview_ = 0;
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
      page_ = (page_ + 1) % kNumOptionPages;
      ResetPots(pots);
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
      const OptionSetting setting = kOptionPages[page_].knobs[i];
      if (setting == OPTION_RESERVED) {
        continue;
      }

      const float pot = pots[i];
      bool moved = false;
      if (!qualified_[i] &&
          fabsf(pot - origin_[i]) > kOptionsPickupThreshold) {
        moved = true;
        qualified_[i] = true;
        zone_[i] = pot < kOptionsZoneLow ? 0 : pot < kOptionsZoneHigh ? 1 : 2;
        selected_ = setting;
      }

      if (!qualified_[i]) {
        continue;
      }

      if (fabsf(pot - previous_[i]) > kOptionsMovementThreshold) {
        moved = true;
        previous_[i] = pot;
        selected_ = setting;
        last_action_ = now;
      }

      const int before = Value(setting);
      UpdateZone(pot, &zone_[i]);
      switch (setting) {
        case OPTION_PITCH:
          pitch_preview_ = zone_[i] == 2 ? scale_ : PitchScale(zone_[i]);
          break;
        case OPTION_WIDTH: {
          if (!moved) {
            break;
          }

          bool enabled = width_preview_ != 0;
          if (pot < kWidthStockThreshold) {
            enabled = false;
          }

          if (pot > kWidthEnabledThreshold) {
            enabled = true;
          }

          float amount = (pot - kWidthEnabledThreshold) / kWidthRange;
          CONSTRAIN(amount, 0.0f, 1.0f);
          width_preview_ = enabled
              ? 1 + int(amount * float(kStereoWidthMax - 1) + 0.5f) : 0;
        } break;
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
        case OPTION_ENGINE:
          engine_preview_ = Engine(zone_[i]);
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
    eligible_ = armed_ && normal && !other && !pot_touched;
    armed_ = false;
    press_time_ = now;
  }

  if (other || pot_touched || !normal) {
    eligible_ = false;
  }

  if (eligible_ && assign &&
      uint32_t(now - press_time_) >= kOptionsHoldTime) {
    limiter_preview_ = settings.output_limiter_mode();
    engine_preview_ = settings.engine();
    clouds_preview_ = settings.clouds_style();
    plaits_preview_ = settings.plaits_synth();
    pitch_preview_ = settings.pitch_scale();
    scale_ = settings.pitch_scale() >= PITCH_SCALE_FIFTHS
        ? settings.pitch_scale() : PITCH_SCALE_FIFTHS;
    width_preview_ = settings.stereo_width();
    delay_preview_ = settings.delay_mode();
    evolving_preview_ = settings.evolving_feedback();
    attack_preview_ = settings.attack_mode();
    ResetPots(pots);
    eligible_ = false;
    stage_ = STAGE_WAIT_RELEASE;
  }
}

int Options::Value(OptionSetting setting) const {
  switch (setting) {
    case OPTION_PITCH:
      return pitch_preview_;
    case OPTION_WIDTH:
      return width_preview_;
    case OPTION_REVERB:
      return clouds_preview_;
    case OPTION_DELAY:
      return delay_preview_;
    case OPTION_ENGINE:
      return engine_preview_;
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
  int value = Value(selected_);
  if (selected_ == OPTION_WIDTH && value) {
    value = 1;
  }

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
    selected_ = OPTION_PITCH;
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
    if (kOptionPages[page_].knobs[i] != OPTION_RESERVED) {
      selected_ = kOptionPages[page_].knobs[i];
      break;
    }
  }

  seed_held_ = false;
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
