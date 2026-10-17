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

#include "beads/dsp/limiter.h"
#include "beads/dsp/playback_options.h"
#include "beads/dsp/pitch_quantizer.h"

namespace beads {

// Category marked by the dry/wet LED: the limiter is set with DRY/WET.
const int kLimiterPage = 1;
const int kSelfPlayPage = 0;
const int kPitchScalePage = 2;

class Options {
 public:
  enum Stage {
    STAGE_INACTIVE,
    STAGE_WAIT_RELEASE,
    STAGE_EDITING,
    STAGE_DRAIN
  };

  Options() { }
  ~Options() { }

  void Init() {
    stage_ = STAGE_INACTIVE;
    armed_ = eligible_ = holding_ = commit_ = false;
    all_up_time_ = last_action_ = press_time_ = 0;
    page_ = zone_ = 0;
    qualified_ = false;
    origin_ = previous_ = 0.0f;
    preview_ = OUTPUT_LIMITER_STOCK;
    self_play_preview_ = false;
    clouds_preview_ = false;
    reverb_qualified_ = false;
    reverb_selected_ = false;
    reverb_origin_ = 0.0f;
    reverb_previous_ = 0.0f;
    pitch_preview_ = PITCH_SCALE_OFF;
    scale_ = PITCH_SCALE_FIFTHS;
    seed_held_ = false;
    seed_time_ = 0;
    width_preview_ = STEREO_WIDTH_STOCK;
    width_zone_ = 0;
    delay_preview_ = DELAY_MODE_AUTO;
    delay_zone_ = 0;
    delay_origin_ = delay_previous_ = 0.0f;
    delay_qualified_ = delay_selected_ = false;
  }

  void Poll(
      uint32_t now, bool assign, bool other, bool all_up,
      bool just_pressed, bool normal, bool pot_touched, float pot,
      OutputLimiterMode committed, float feedback, bool self_play,
      float reverb, bool clouds, PitchScale pitch, bool seed,
      bool seed_pressed, StereoWidth width, DelayMode delay) {
    const float dry_wet = pot;
    if (stage_ != STAGE_INACTIVE && page_ != kLimiterPage) {
      pot = feedback;
    }
    if (stage_ == STAGE_DRAIN) {
      if (all_up && !commit_) {
        stage_ = STAGE_INACTIVE;
        all_up_time_ = now;
        armed_ = false;
      }
      return;
    }
    if (stage_ != STAGE_INACTIVE) {
      if (other || !normal) {
        Finish(false, committed, self_play, clouds, pitch, width, delay);
        return;
      }
      if (uint32_t(now - last_action_) >= 5000) {
        // Idle exit keeps a changed value, like the hold exit.
        Finish(preview_ != committed || self_play_preview_ != self_play ||
            clouds_preview_ != clouds || pitch_preview_ != pitch ||
            width_preview_ != width || delay_preview_ != delay,
            committed, self_play, clouds, pitch, width, delay);
        return;
      }
      if (stage_ == STAGE_WAIT_RELEASE) {
        if (all_up) {
          stage_ = STAGE_EDITING;
          holding_ = false;
          last_action_ = now;
        }
        return;
      }
      if (just_pressed) {
        holding_ = true;
        press_time_ = last_action_ = now;
      }
      if (holding_ && assign && uint32_t(now - press_time_) >= 3000) {
        Finish(true, committed, self_play, clouds, pitch, width, delay);
        return;
      }
      if (holding_ && all_up) {
        holding_ = false;
        page_ = (page_ + 1) % 3;
        pot = page_ != kLimiterPage ? feedback : dry_wet;
        ResetPot(pot);
        ResetReverb(reverb);
        ResetDelay(dry_wet);
        last_action_ = now;
      }
      if (page_ == kSelfPlayPage) {
        if (!delay_qualified_ && fabsf(dry_wet - delay_origin_) > 0.10f) {
          delay_qualified_ = true;
          delay_zone_ = dry_wet < 1.0f / 3.0f ? 0
              : dry_wet < 2.0f / 3.0f ? 1 : 2;
        }
        if (fabsf(dry_wet - delay_previous_) > 0.01f) {
          last_action_ = now;
          delay_previous_ = dry_wet;
          delay_selected_ = true;
          reverb_selected_ = false;
        }
        if (delay_qualified_) {
          UpdateZone(dry_wet, &delay_zone_);
          delay_preview_ = DelayMode(delay_zone_);
        }
      }
      if (page_ == kLimiterPage || page_ == kSelfPlayPage) {
        if (!reverb_qualified_ && fabsf(reverb - reverb_origin_) > 0.10f) {
          reverb_qualified_ = true;
          width_zone_ = reverb >= 0.10f ? 1 : 0;
        }
        if (fabsf(reverb - reverb_previous_) > 0.01f) {
          last_action_ = now;
          reverb_previous_ = reverb;
          reverb_selected_ = true;
          delay_selected_ = false;
        }
        if (reverb_qualified_ && page_ == kSelfPlayPage) {
          if (reverb < 0.07f) width_zone_ = 0;
          if (reverb > 0.13f) width_zone_ = 1;
          float amount = (reverb - 0.13f) / 0.87f;
          CONSTRAIN(amount, 0.0f, 1.0f);
          width_preview_ = width_zone_ ? 1 + int(amount * 254.0f) : 0;
        }
        if (reverb_qualified_ && page_ == kLimiterPage) {
          if (reverb < 0.5f - 0.03f) {
            clouds_preview_ = false;
          } else if (reverb > 0.5f + 0.03f) {
            clouds_preview_ = true;
          }
        }
      }
      if (!qualified_ && fabsf(pot - origin_) > 0.10f) {
        qualified_ = true;
        zone_ = pot < 1.0f / 3.0f ? 0 : pot < 2.0f / 3.0f ? 1 : 2;
      }
      if (fabsf(pot - previous_) > 0.01f) {
        last_action_ = now;
        previous_ = pot;
        reverb_selected_ = false;
        delay_selected_ = false;
      }
      if (qualified_ && page_ == kSelfPlayPage) {
        if (pot < 0.5f - 0.03f) {
          self_play_preview_ = false;
        } else if (pot > 0.5f + 0.03f) {
          self_play_preview_ = true;
        }
      } else if (qualified_) {
        const int previous_zone = zone_;
        if (pot < 1.0f / 3.0f - 0.03f) {
          zone_ = 0;
        } else if (pot > 2.0f / 3.0f + 0.03f) {
          zone_ = 2;
        } else if ((zone_ == 0 && pot > 1.0f / 3.0f + 0.03f) ||
                   (zone_ == 2 && pot < 2.0f / 3.0f - 0.03f)) {
          zone_ = 1;
        }
        if (page_ == kLimiterPage) {
          preview_ = OutputLimiterMode(zone_);
        } else if (page_ == kPitchScalePage) {
          if (previous_zone != zone_) {
            seed_held_ = false;
          }
          pitch_preview_ = zone_ == 2 ? scale_ : PitchScale(zone_);
        }
      }
      if (seed_allowed() && !assign) {
        if (seed) {
          if (seed_pressed) {
            seed_time_ = now;
            seed_held_ = true;
          }
          last_action_ = now;
        } else if (seed_held_) {
          if (uint32_t(now - seed_time_) < 500) {
            scale_ = scale_ == PITCH_SCALE_SHRI ? PITCH_SCALE_FIFTHS
                : PitchScale(scale_ + 1);
            pitch_preview_ = scale_;
          }
          seed_held_ = false;
          last_action_ = now;
        }
      } else {
        seed_held_ = false;
      }
      return;
    }
    if (all_up) {
      if (uint32_t(now - all_up_time_) >= 20) {
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
    if (eligible_ && assign && uint32_t(now - press_time_) >= 3000) {
      stage_ = STAGE_WAIT_RELEASE;
      preview_ = committed;
      self_play_preview_ = self_play;
      clouds_preview_ = clouds;
      width_preview_ = width;
      delay_preview_ = delay;
      ResetReverb(reverb);
      ResetDelay(dry_wet);
      pitch_preview_ = pitch;
      scale_ = pitch >= PITCH_SCALE_FIFTHS ? pitch : PITCH_SCALE_FIFTHS;
      page_ = kPitchScalePage;
      ResetPot(feedback);
      last_action_ = now;
      eligible_ = false;
    }
  }

  inline DelayMode delay_preview() const { return delay_preview_; }
  inline bool delay_selected() const { return delay_selected_; }
  inline StereoWidth width_preview() const { return width_preview_; }
  inline PitchScale pitch_preview() const { return pitch_preview_; }
  inline bool seed_allowed() const {
    return stage_ == STAGE_EDITING && page_ == kPitchScalePage &&
        pitch_preview_ >= PITCH_SCALE_FIFTHS;
  }
  inline bool active() const { return stage_ != STAGE_INACTIVE; }
  inline Stage stage() const { return stage_; }
  inline int page() const { return page_; }
  inline OutputLimiterMode preview() const { return preview_; }
  inline bool self_play_preview() const { return self_play_preview_; }
  inline bool clouds_preview() const { return clouds_preview_; }
  inline bool reverb_selected() const { return reverb_selected_; }
  inline bool commit_requested() const { return commit_; }
  inline void ClearCommit() { commit_ = false; }

 private:
  void UpdateZone(float pot, int* zone) {
    if (pot < 1.0f / 3.0f - 0.03f) {
      *zone = 0;
    } else if (pot > 2.0f / 3.0f + 0.03f) {
      *zone = 2;
    } else if ((*zone == 0 && pot > 1.0f / 3.0f + 0.03f) ||
               (*zone == 2 && pot < 2.0f / 3.0f - 0.03f)) {
      *zone = 1;
    }
  }

  void ResetDelay(float pot) {
    delay_origin_ = delay_previous_ = pot;
    delay_qualified_ = delay_selected_ = false;
  }

  void ResetReverb(float reverb) {
    reverb_origin_ = reverb;
    reverb_previous_ = reverb;
    reverb_qualified_ = false;
    reverb_selected_ = false;
  }

  void ResetPot(float pot) {
    origin_ = previous_ = pot;
    qualified_ = false;
    seed_held_ = false;
  }
  void Finish(
      bool commit, OutputLimiterMode committed, bool self_play,
      bool clouds, PitchScale pitch, StereoWidth width, DelayMode delay) {
    if (!commit) {
      preview_ = committed;
      self_play_preview_ = self_play;
      clouds_preview_ = clouds;
      width_preview_ = width;
      delay_preview_ = delay;
      pitch_preview_ = pitch;
      scale_ = pitch >= PITCH_SCALE_FIFTHS ? pitch : PITCH_SCALE_FIFTHS;
    }
    commit_ = commit;
    stage_ = STAGE_DRAIN;
    holding_ = false;
  }

  // Published by SysTick; main reads it before the entry flush request.
  volatile Stage stage_;
  bool armed_;
  bool eligible_;
  bool holding_;
  volatile bool commit_;
  bool qualified_;
  uint32_t all_up_time_;
  uint32_t last_action_;
  uint32_t press_time_;
  int page_;
  int zone_;
  float origin_;
  float previous_;
  OutputLimiterMode preview_;
  bool self_play_preview_;
  bool clouds_preview_;
  StereoWidth width_preview_;
  DelayMode delay_preview_;
  int delay_zone_;
  float delay_origin_;
  float delay_previous_;
  bool delay_qualified_;
  bool delay_selected_;
  int width_zone_;
  bool reverb_qualified_;
  bool reverb_selected_;
  float reverb_origin_;
  float reverb_previous_;
  PitchScale pitch_preview_;
  PitchScale scale_;
  bool seed_held_;
  uint32_t seed_time_;

  DISALLOW_COPY_AND_ASSIGN(Options);
};

}  // namespace beads

#endif  // BEADS_OPTIONS_H_
