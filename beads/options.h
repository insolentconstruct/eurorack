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

#include <algorithm>
#include <cmath>
#include "stmlib/stmlib.h"
#include "beads/dsp/limiter.h"
#include "beads/dsp/playback_options.h"
#include "beads/dsp/pitch_quantizer.h"

namespace beads {

enum OptionSetting {
  OPTION_RESERVED, OPTION_PITCH, OPTION_WIDTH, OPTION_REVERB,
  OPTION_DELAY, OPTION_SELF_PLAY, OPTION_LIMITER
};
struct OptionPage {
  const char* led;
  int led_index;
  OptionSetting knobs[3];  // FEEDBACK, DRY/WET, REVERB.
};
const int kPitchScalePage = 0;
const uint32_t kOptionsSettleTime = 1000;
const OptionPage kOptionPages[3] = {
  { "REVERB", 2, { OPTION_PITCH, OPTION_WIDTH, OPTION_REVERB } },
  { "FEEDBACK", 0, { OPTION_RESERVED, OPTION_DELAY, OPTION_RESERVED } },
  { "DRY/WET", 1, { OPTION_SELF_PLAY, OPTION_LIMITER, OPTION_RESERVED } }
};
const char* const kOptionNames[] = {
  "Reserved", "Pitch scale", "Stereo width", "Reverb character",
  "Delay mode", "Self-play", "Limiter"
};
const char* const kOptionKnobNames[] = { "FEEDBACK", "DRY/WET", "REVERB" };
// Shared by the MEDIA renderer, host export and documentation check.
struct OptionValue {
  OptionSetting setting;
  int value;
  const char* name;
  const char* colour;
  uint8_t r, g, b;
  int pulses;  // 0 steady, 1 slow, 2 fast, 3 yellow double pulse.
};
const OptionValue kOptionValues[] = {
  { OPTION_RESERVED, 0, "Reserved", "unchanged", 0, 0, 0, 0 },
  { OPTION_PITCH, 0, "Off", "green", 0, 255, 0, 0 },
  { OPTION_PITCH, 1, "Chromatic", "yellow", 255, 255, 0, 0 },
  { OPTION_PITCH, 2, "Fifths and octaves", "red", 255, 0, 0, 0 },
  { OPTION_PITCH, 3, "Major", "green", 0, 255, 0, 1 },
  { OPTION_PITCH, 4, "Minor", "yellow", 255, 255, 0, 1 },
  { OPTION_PITCH, 5, "Major pentatonic", "red", 255, 0, 0, 1 },
  { OPTION_PITCH, 6, "Minor pentatonic", "green", 0, 255, 0, 2 },
  { OPTION_PITCH, 7, "Pelog", "yellow", 255, 255, 0, 2 },
  { OPTION_PITCH, 8, "Raag Bhairav", "red", 255, 0, 0, 2 },
  { OPTION_PITCH, 9, "Raag Shri", "yellow", 255, 255, 0, 3 },
  { OPTION_WIDTH, 0, "Stock", "green", 0, 255, 0, 0 },
  { OPTION_WIDTH, 1, "Enabled (1..255)", "meter", 255, 255, 0, 1 },
  { OPTION_REVERB, 0, "Stock", "green", 0, 255, 0, 0 },
  { OPTION_REVERB, 1, "Clouds-style", "yellow", 255, 255, 0, 0 },
  { OPTION_DELAY, 0, "Auto (Stock)", "green", 0, 255, 0, 0 },
  { OPTION_DELAY, 1, "Never", "yellow", 255, 255, 0, 0 },
  { OPTION_DELAY, 2, "Wide detent", "red", 255, 0, 0, 0 },
  { OPTION_SELF_PLAY, 0, "Automatic (Stock)", "green", 0, 255, 0, 0 },
  { OPTION_SELF_PLAY, 1, "On request", "yellow", 255, 255, 0, 0 },
  { OPTION_LIMITER, 0, "Stock", "green", 0, 255, 0, 0 },
  { OPTION_LIMITER, 1, "Fast release", "yellow", 255, 255, 0, 0 },
  { OPTION_LIMITER, 2, "Off", "red", 255, 0, 0, 0 }
};
const size_t kNumOptionValues = sizeof(kOptionValues) / sizeof(kOptionValues[0]);

class Options {
 public:
  enum Stage { STAGE_INACTIVE, STAGE_WAIT_RELEASE, STAGE_EDITING, STAGE_DRAIN };
  Options() { }
  ~Options() { }
  void Init() {
    stage_ = STAGE_INACTIVE;
    armed_ = eligible_ = holding_ = commit_ = dirty_ = false;
    all_up_time_ = press_time_ = last_action_ = 0;
    page_ = 0;
    selected_ = OPTION_PITCH;
    preview_ = OUTPUT_LIMITER_STOCK;
    self_play_preview_ = clouds_preview_ = false;
    pitch_preview_ = PITCH_SCALE_OFF;
    scale_ = PITCH_SCALE_FIFTHS;
    width_preview_ = 0;
    delay_preview_ = DELAY_MODE_AUTO;
    seed_held_ = false;
    seed_time_ = 0;
    for (int i = 0; i < 3; ++i) {
      origin_[i] = previous_[i] = 0.0f;
      qualified_[i] = false;
      zone_[i] = 0;
    }
  }
  void Poll(uint32_t now, bool assign, bool other, bool all_up,
      bool just_pressed, bool normal, bool pot_touched, float dry_wet,
      OutputLimiterMode committed, float feedback, bool self_play,
      float reverb, bool clouds, PitchScale pitch, bool seed,
      bool seed_pressed, StereoWidth width, DelayMode delay) {
    const float pots[3] = { feedback, dry_wet, reverb };
    if (stage_ == STAGE_DRAIN) {
      RequestCommit(now);
      if (all_up && !dirty_ && !commit_) {
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
        if (all_up) { stage_ = STAGE_EDITING; holding_ = false; }
        return;
      }
      if (just_pressed) { holding_ = true; press_time_ = now; }
      if (holding_ && assign && uint32_t(now - press_time_) >= 3000) {
        Finish();
        RequestCommit(now);
        return;
      }
      if (holding_ && all_up) {
        holding_ = false;
        page_ = (page_ + 1) % 3;
        ResetPots(pots);
      }
      // Main consumes a stable batch. Navigation and exit still work while
      // flash is busy; only edits wait for ClearCommit.
      if (commit_) return;
      for (int i = 0; i < 3; ++i) {
        const OptionSetting setting = kOptionPages[page_].knobs[i];
        if (setting == OPTION_RESERVED) continue;
        const float pot = pots[i];
        if (!qualified_[i] && fabsf(pot - origin_[i]) > 0.10f) {
          qualified_[i] = true;
          zone_[i] = pot < 1.0f / 3.0f ? 0 : pot < 2.0f / 3.0f ? 1 : 2;
          selected_ = setting;
        }
        if (!qualified_[i]) continue;
        if (fabsf(pot - previous_[i]) > 0.005f) {
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
            int enabled = width_preview_ != 0;
            if (pot < 0.07f) enabled = 0;
            if (pot > 0.13f) enabled = 1;
            float amount = (pot - 0.13f) / 0.87f;
            CONSTRAIN(amount, 0.0f, 1.0f);
            width_preview_ = enabled ? 1 + int(amount * 254.0f + 0.5f) : 0;
          } break;
          case OPTION_REVERB:
            if (pot < 0.47f) clouds_preview_ = false;
            if (pot > 0.53f) clouds_preview_ = true;
            break;
          case OPTION_DELAY: delay_preview_ = DelayMode(zone_[i]); break;
          case OPTION_SELF_PLAY:
            if (pot < 0.47f) self_play_preview_ = false;
            if (pot > 0.53f) self_play_preview_ = true;
            break;
          case OPTION_LIMITER: preview_ = OutputLimiterMode(zone_[i]); break;
          default: break;
        }
        if (before != Value(setting)) { dirty_ = true; last_action_ = now; }
      }
      if (seed_allowed() && !assign) {
        if (seed_pressed) { seed_time_ = now; seed_held_ = true; }
        if (!seed && seed_held_) {
          if (uint32_t(now - seed_time_) < 500) {
            scale_ = scale_ == PITCH_SCALE_SHRI ? PITCH_SCALE_FIFTHS
                : PitchScale(scale_ + 1);
            pitch_preview_ = scale_;
            selected_ = OPTION_PITCH;
            dirty_ = true;
            last_action_ = now;
          }
          seed_held_ = false;
        }
      } else { seed_held_ = false; }
      RequestCommit(now);
      return;
    }
    if (all_up) {
      if (uint32_t(now - all_up_time_) >= 20) armed_ = true;
      eligible_ = false;
    } else { all_up_time_ = now; }
    if (just_pressed) {
      eligible_ = armed_ && normal && !other && !pot_touched;
      armed_ = false;
      press_time_ = now;
    }
    if (other || pot_touched || !normal) eligible_ = false;
    if (eligible_ && assign && uint32_t(now - press_time_) >= 3000) {
      preview_ = committed;
      self_play_preview_ = self_play;
      clouds_preview_ = clouds;
      pitch_preview_ = pitch;
      scale_ = pitch >= PITCH_SCALE_FIFTHS ? pitch : PITCH_SCALE_FIFTHS;
      width_preview_ = width;
      delay_preview_ = delay;
      ResetPots(pots);
      eligible_ = false;
      stage_ = STAGE_WAIT_RELEASE;
    }
  }
  int Value(OptionSetting setting) const {
    switch (setting) {
      case OPTION_PITCH: return pitch_preview_;
      case OPTION_WIDTH: return width_preview_;
      case OPTION_REVERB: return clouds_preview_;
      case OPTION_DELAY: return delay_preview_;
      case OPTION_SELF_PLAY: return self_play_preview_;
      case OPTION_LIMITER: return preview_;
      default: return 0;
    }
  }
  const OptionValue& indication() const {
    int value = Value(selected_);
    if (selected_ == OPTION_WIDTH && value) value = 1;
    for (size_t i = 0; i < kNumOptionValues; ++i) {
      if (kOptionValues[i].setting == selected_ && kOptionValues[i].value == value)
        return kOptionValues[i];
    }
    return kOptionValues[0];
  }
  inline bool active() const { return stage_ != STAGE_INACTIVE; }
  inline Stage stage() const { return stage_; }
  inline int page() const { return page_; }
  inline OutputLimiterMode preview() const { return preview_; }
  inline bool self_play_preview() const { return self_play_preview_; }
  inline bool clouds_preview() const { return clouds_preview_; }
  inline StereoWidth width_preview() const { return width_preview_; }
  inline DelayMode delay_preview() const { return delay_preview_; }
  inline PitchScale pitch_preview() const { return pitch_preview_; }
  inline bool seed_allowed() const {
    return stage_ == STAGE_EDITING && page_ == kPitchScalePage &&
        pitch_preview_ >= PITCH_SCALE_FIFTHS;
  }
  inline bool commit_requested() const { return commit_; }
  inline void ClearCommit() { dirty_ = false; commit_ = false; }
 private:
  void RequestCommit(uint32_t now) {
    if (!commit_ && dirty_ && uint32_t(now - last_action_) >= kOptionsSettleTime) {
      commit_ = true;
    }
  }
  void ResetPots(const float* pots) {
    for (int i = 0; i < 3; ++i) {
      origin_[i] = previous_[i] = pots[i];
      qualified_[i] = false;
    }
    for (int i = 0; i < 3; ++i) {
      if (kOptionPages[page_].knobs[i] != OPTION_RESERVED) {
        selected_ = kOptionPages[page_].knobs[i];
        break;
      }
    }
    seed_held_ = false;
  }
  void UpdateZone(float pot, int* zone) {
    if (pot < 1.0f / 3.0f - 0.03f) *zone = 0;
    else if (pot > 2.0f / 3.0f + 0.03f) *zone = 2;
    else if ((*zone == 0 && pot > 1.0f / 3.0f + 0.03f) ||
             (*zone == 2 && pot < 2.0f / 3.0f - 0.03f)) *zone = 1;
  }
  void Finish() { stage_ = STAGE_DRAIN; holding_ = false; seed_held_ = false; }
  // SysTick publishes transitions and a stable batch to main.
  volatile Stage stage_;
  volatile bool commit_;
  bool dirty_;
  bool armed_;
  bool eligible_;
  bool holding_;
  uint32_t all_up_time_;
  uint32_t press_time_;
  uint32_t last_action_;
  int page_;
  OptionSetting selected_;
  float origin_[3];
  float previous_[3];
  bool qualified_[3];
  int zone_[3];
  OutputLimiterMode preview_;
  bool self_play_preview_;
  bool clouds_preview_;
  StereoWidth width_preview_;
  DelayMode delay_preview_;
  PitchScale pitch_preview_;
  PitchScale scale_;
  bool seed_held_;
  uint32_t seed_time_;
  DISALLOW_COPY_AND_ASSIGN(Options);
};
}  // namespace beads
#endif  // BEADS_OPTIONS_H_
