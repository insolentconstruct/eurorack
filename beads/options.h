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
#include "beads/dsp/limiter.h"

namespace beads {

// Category marked by the dry/wet LED: the limiter is set with DRY/WET.
const int kLimiterPage = 1;

class Options {
 public:
  enum Stage { INACTIVE, WAIT_RELEASE, EDITING, DRAIN };

  void Init() {
    stage_ = INACTIVE;
    armed_ = eligible_ = holding_ = commit_ = false;
    all_up_time_ = last_action_ = press_time_ = 0;
    page_ = zone_ = 0;
    qualified_ = false;
    origin_ = previous_ = 0.0f;
    preview_ = OUTPUT_LIMITER_STOCK;
  }

  void Poll(
      uint32_t now, bool assign, bool other, bool all_up,
      bool just_pressed, bool normal, bool pot_touched, float pot,
      OutputLimiterMode committed) {
    if (stage_ == DRAIN) {
      if (all_up && !commit_) {
        stage_ = INACTIVE;
        all_up_time_ = now;
        armed_ = false;
      }
      return;
    }
    if (stage_ != INACTIVE) {
      if (other || !normal) {
        Finish(false, committed);
        return;
      }
      if (uint32_t(now - last_action_) >= 5000) {
        // Idle exit keeps a changed value, like the hold exit.
        Finish(preview_ != committed, committed);
        return;
      }
      if (stage_ == WAIT_RELEASE) {
        if (all_up) {
          stage_ = EDITING;
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
        Finish(true, committed);
        return;
      }
      if (holding_ && all_up) {
        holding_ = false;
        page_ = (page_ + 1) % 3;
        ResetPot(pot);
        last_action_ = now;
      }
      if (!qualified_ && fabsf(pot - origin_) > 0.10f) {
        qualified_ = true;
        zone_ = pot < 1.0f / 3.0f ? 0 : pot < 2.0f / 3.0f ? 1 : 2;
      }
      if (fabsf(pot - previous_) > 0.01f) {
        last_action_ = now;
        previous_ = pot;
      }
      if (qualified_) {
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
        }
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
      stage_ = WAIT_RELEASE;
      preview_ = committed;
      page_ = kLimiterPage;
      ResetPot(pot);
      last_action_ = now;
      eligible_ = false;
    }
  }

  inline bool active() const { return stage_ != INACTIVE; }
  inline Stage stage() const { return stage_; }
  inline int page() const { return page_; }
  inline OutputLimiterMode preview() const { return preview_; }
  inline bool commit_requested() const { return commit_; }
  inline void ClearCommit() { commit_ = false; }

 private:
  void ResetPot(float pot) {
    origin_ = previous_ = pot;
    qualified_ = false;
  }
  void Finish(bool commit, OutputLimiterMode committed) {
    if (!commit) {
      preview_ = committed;
    }
    commit_ = commit;
    stage_ = DRAIN;
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
};

}  // namespace beads

#endif  // BEADS_OPTIONS_H_
