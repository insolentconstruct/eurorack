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
// Guarded page, UI and STAT storage tests.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>

#include "beads/test/test_checker.h"

#define private public
#include "beads/settings.h"
#include "beads/ui.h"
#include "beads/normalization_detector.h"
#undef private

#include "beads/background_buffer_backup.h"
#include "beads/calibrator.h"
#include "beads/dsp/limiter.h"
#include "beads/gain_control.h"

// Keep the detector in this harness without changing its build recipe.
#include "beads/normalization_detector.cc"

bool test_down[4] = { };
uint32_t test_writes = 0;
#include "beads/test/firmware_shims.h"

using namespace beads;

STATIC_ASSERT(sizeof(State) == 24, stat_size);
STATIC_ASSERT(offsetof(State, media) == 0, media);
STATIC_ASSERT(offsetof(State, assignable_cv_destination) == 1, assignment);
STATIC_ASSERT(offsetof(State, assignable_cv_amount) == 2, cv_amounts);
STATIC_ASSERT(offsetof(State, gain) == 5, gain);
STATIC_ASSERT(offsetof(State, gate_output_mode) == 6, gate);
STATIC_ASSERT(offsetof(State, frozen) == 7, frozen);
STATIC_ASSERT(offsetof(State, disable_autosave) == 8, autosave);
STATIC_ASSERT(offsetof(State, monophonic_mode) == 9, mono);
namespace state_layout_test {
STATIC_ASSERT(offsetof(State, diffusion) == 17, diffusion);
STATIC_ASSERT(offsetof(State, frozen_feedback) == 18, frozen_feedback);
STATIC_ASSERT(offsetof(State, attacks) == 19, attacks);
STATIC_ASSERT(offsetof(State, engine) == 20, engine);
STATIC_ASSERT(offsetof(State, synth_engine) == 21, synth_engine);
STATIC_ASSERT(offsetof(State, random_intervals) == 22, random_intervals);
STATIC_ASSERT(offsetof(State, padding) == 23, padding);
}

struct Rig {
  Settings settings;
  GainControl gain;
  Calibrator calibration;
  BackgroundBufferBackup backup;
  Ui ui;
  float pot;
  float feedback;
  float reverb;

  Rig(bool startup_assign = false)
      : pot(0.5f), feedback(0.5f), reverb(0.5f) {
    std::fill(test_down, test_down + 4, false);
    test_down[SWITCH_ASSIGN] = startup_assign;
    memset(reinterpret_cast<void*>(0x081c0000), 255, 0x40000);
    settings.Init(false);
    ui.Init(&settings, &gain, &calibration, &backup);
    ui.LoadOptions();
    Tick(40);
  }
  void Tick(int n, bool events = true) {
    while (n--) {
      ui.ProcessPot(Ui::MIX_PARAMETER_FEEDBACK, feedback);
      ui.ProcessPot(Ui::MIX_PARAMETER_DRY_WET, pot);
      ui.ProcessPot(Ui::MIX_PARAMETER_REVERB, reverb);
      ui.Poll();
      if (events) {
        ui.DoEvents();
      }
    }
  }
  void Down(Switch s) {
    test_down[s] = true;
    Tick(8);
  }
  void Up(Switch s) {
    test_down[s] = false;
    Tick(8);
  }
  void Enter() {
    Down(SWITCH_ASSIGN);
    Tick(2999);
    CHECK(ui.options_.stage() == Options::STAGE_WAIT_RELEASE);
    Up(SWITCH_ASSIGN);
    CHECK(ui.options_.stage() == Options::STAGE_EDITING);
  }
  void EnterPage(int page) {
    Enter();
    while (page--) {
      Tap();
    }
  }
  void EnterLimiter() {
    EnterPage(3);
  }
  void Tap() {
    Down(SWITCH_ASSIGN);
    Up(SWITCH_ASSIGN);
  }
};

void TestTrimGesture() {
  test_checker.Start("TestTrimGesture");
  for (int order = 0; order < 2; ++order) {
    Rig r;
    r.Down(SWITCH_FREEZE);
    r.Up(SWITCH_FREEZE);
    CHECK(r.ui.freeze_button_state_);
    const uint8_t media = r.settings.state().media;
    r.Down(SWITCH_MEDIA);
    r.Down(SWITCH_FREEZE);
    CHECK(r.ui.trim_flag_);
    CHECK(!r.ui.freeze_button_state_);
    CHECK(r.ui.ignore_release_[SWITCH_MEDIA]);
    CHECK(r.ui.ignore_release_[SWITCH_FREEZE]);
    r.ui.ClearTrimFlag();
    CHECK(!r.ui.trim_flag_);
    r.Up(order ? SWITCH_MEDIA : SWITCH_FREEZE);
    r.Up(order ? SWITCH_FREEZE : SWITCH_MEDIA);
    CHECK(!r.ui.freeze_button_state_);
    CHECK(r.settings.state().media == media);
  }
  Rig reverse;
  const uint8_t media = reverse.settings.state().media;
  reverse.Down(SWITCH_FREEZE);
  reverse.Down(SWITCH_MEDIA);
  CHECK(!reverse.ui.trim_flag_);
  reverse.Up(SWITCH_FREEZE);
  reverse.Up(SWITCH_MEDIA);
  CHECK(reverse.ui.freeze_button_state_);
  CHECK(reverse.settings.state().media == (media + 3) % 4);
  Rig page;
  page.EnterLimiter();
  const uint8_t page_media = page.settings.state().media;
  test_down[SWITCH_MEDIA] = true;
  page.Tick(8, false);
  CHECK(!page.ui.queue_.available());
  test_down[SWITCH_FREEZE] = true;
  page.Tick(8, false);
  CHECK(!page.ui.queue_.available());
  page.ui.DoEvents();
  page.Up(SWITCH_FREEZE);
  page.Up(SWITCH_MEDIA);
  CHECK(!page.ui.trim_flag_);
  CHECK(!page.ui.freeze_button_state_);
  CHECK(page.settings.state().media == page_media);
}

void TestMonophonicGesture() {
  test_checker.Start("TestMonophonicGesture");
  for (int latched = 0; latched < 2; ++latched) {
    for (int order = 0; order < 2; ++order) {
      Rig r;
      CHECK(!r.settings.state().monophonic_mode);
      if (!latched) {
        r.Down(SWITCH_SEED);
        r.Up(SWITCH_SEED);
      }
      const bool seed = r.ui.seed_button_state_;
      const uint8_t media = r.settings.state().media;
      uint32_t writes = test_writes;
      for (int toggle = 0; toggle < 2; ++toggle) {
        r.Down(SWITCH_MEDIA);
        r.Down(SWITCH_SEED);
        CHECK(r.settings.state().monophonic_mode == !toggle);
        CHECK(test_writes == ++writes);
        r.Tick(1600);
        r.Up(order ? SWITCH_MEDIA : SWITCH_SEED);
        r.Up(order ? SWITCH_SEED : SWITCH_MEDIA);
        CHECK(test_writes == writes);
        CHECK(r.settings.state().media == media);
        CHECK(r.gain.restarts == 0);
        CHECK(r.ui.seed_button_locked_ == bool(latched));
        CHECK(r.ui.seed_button_state_ == seed);
        Settings loaded;
        CHECK(loaded.Init(false));
        CHECK(loaded.state().monophonic_mode == !toggle);
        r.ui.set_overwrite_reserve(1.0f);
        r.Tick(1);
        Leds::Color color = r.ui.media_palette_[media];
        const float brightness = toggle ? 1.0f : 0.5f;
        CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] ==
            uint8_t(color.r * brightness * brightness));
        CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] ==
            uint8_t(color.g * brightness));
        CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] ==
            uint8_t(color.b * brightness));
      }
    }
  }
  Rig reverse;
  const uint8_t media = reverse.settings.state().media;
  reverse.Down(SWITCH_SEED);
  CHECK(!reverse.ui.seed_button_locked_);
  CHECK(reverse.ui.seed_button_state_);
  reverse.Down(SWITCH_MEDIA);
  reverse.Up(SWITCH_SEED);
  reverse.Up(SWITCH_MEDIA);
  CHECK(!reverse.settings.state().monophonic_mode);
  CHECK(!reverse.ui.seed_button_state_);
  CHECK(reverse.settings.state().media == (media + 3) % 4);
  Rig page;
  page.EnterLimiter();
  page.Down(SWITCH_MEDIA);
  page.Down(SWITCH_SEED);
  page.Up(SWITCH_SEED);
  page.Up(SWITCH_MEDIA);
  CHECK(!page.settings.state().monophonic_mode);
}

static Rig* interrupted_rig;
static bool interrupt_session;
namespace beads {

void TestOptionsDispatchInterrupt() {
  if (interrupted_rig) {
    Rig* r = interrupted_rig;
    interrupted_rig = NULL;
    if (interrupt_session) {
      test_down[SWITCH_ASSIGN] = true;
      r->Tick(3010, false);
      CHECK(r->ui.options_.active());
      test_down[SWITCH_ASSIGN] = false;
      r->Tick(10, false);
      test_down[SWITCH_MEDIA] = true;
      r->Tick(8, false);
      test_down[SWITCH_MEDIA] = false;
      r->Tick(8, false);
    } else {
      r->Tick(1, false);  // After flush check, before options_.active().
    }
  }
}

}  // namespace beads

void TestQueueRace() {
  test_checker.Start("TestQueueRace");
  Rig r;
  r.EnterLimiter();
  r.Down(SWITCH_MEDIA);  // Cancellation enters DRAIN with MEDIA down.
  CHECK(r.ui.options_.stage() == Options::STAGE_DRAIN);
  test_down[SWITCH_MEDIA] = false;
  r.Tick(7, false);  // Debounce 0x7f: release; next tick 0xff exits DRAIN.
  CHECK(r.ui.options_.stage() == Options::STAGE_DRAIN);
  uint8_t media = r.settings.state().media;
  uint32_t writes = test_writes;
  interrupted_rig = &r;
  r.ui.DoEvents();
  if (interrupted_rig) { // Fixed ISR did not queue a release to dispatch.
    interrupted_rig = NULL;
    r.Tick(1, false);
  }
  printf("DRAIN release interleaving: media %u -> %u, saves %u\n",
      media, r.settings.state().media, test_writes - writes);
  fflush(stdout);
  CHECK(!r.ui.options_.active());
  CHECK(r.ui.queue_.available() == 0);
  CHECK(r.settings.state().media == media && test_writes == writes);

  // Retain the read-pointer regression: interrupt between available and pull.
  r.ui.options_.stage_ = Options::STAGE_DRAIN;
  r.ui.queue_.AddEvent(stmlib::CONTROL_SWITCH, SWITCH_MEDIA, 1);
  CHECK(r.ui.queue_.available() == 1);
  r.Tick(1, false);
  r.ui.queue_.PullEvent();
  // An ISR Flush would create 15 phantom events.
  CHECK(r.ui.queue_.available() == 0);

  // Pre-entry events remain pending through an entire session without DoEvents.
  Rig delayed;
  delayed.ui.queue_.AddEvent(stmlib::CONTROL_SWITCH, SWITCH_MEDIA, 1);
  test_down[SWITCH_ASSIGN] = true;
  delayed.Tick(3010, false);
  CHECK(delayed.ui.options_.active());
  test_down[SWITCH_ASSIGN] = false;
  delayed.Tick(10, false);
  test_down[SWITCH_MEDIA] = true;
  delayed.Tick(8, false);
  test_down[SWITCH_MEDIA] = false;
  delayed.Tick(8, false);
  CHECK(!delayed.ui.options_.active());
  media = delayed.settings.state().media;
  writes = test_writes;
  delayed.ui.DoEvents();
  CHECK(delayed.ui.queue_.available() == 0);
  CHECK(delayed.settings.state().media == media && test_writes == writes);

  delayed.Tick(30);  // Re-arm after the preceding drain exit.
  // An event already pulled when entry/exit preempts main is rejected too.
  delayed.ui.queue_.AddEvent(stmlib::CONTROL_SWITCH, SWITCH_MEDIA, 1);
  interrupt_session = true;
  interrupted_rig = &delayed;
  delayed.ui.DoEvents();
  interrupt_session = false;
  CHECK(!interrupted_rig && !delayed.ui.options_.active());
  CHECK(delayed.settings.state().media == media && test_writes == writes);
  delayed.Down(SWITCH_MEDIA);
  delayed.Up(SWITCH_MEDIA);
  CHECK(delayed.settings.state().media != media && test_writes == writes + 1);
  puts("queue interrupt/pre-entry/normal dispatch: PASS");
}

struct LegacyState {
  uint8_t media;
  uint8_t assignable_cv_destination;
  uint8_t assignable_cv_amount[3];
  uint8_t gain;  // 0 for auto-gain
  uint8_t gate_output_mode;
  uint8_t frozen;
  uint8_t disable_autosave;
  uint8_t monophonic_mode;
  uint8_t padding[14];
  enum { tag = 0x54415453 };  // STAT
};

void TestStorage() {
  test_checker.Start("TestStorage");
  Settings s;
  memset(reinterpret_cast<void*>(0x081c0000), 255, 0x40000);
  CHECK(!s.Init(false));
  CHECK(s.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  Settings old_record;
  CHECK(old_record.Init(false));
  CHECK(old_record.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  for (int i = 0; i < 256; ++i) {
    s.mutable_state()->output_limiter_mode = i;
    CHECK(s.output_limiter_mode() == (i == 0xa1 ? OUTPUT_LIMITER_FAST :
        i == 0xa2 ? OUTPUT_LIMITER_OFF : OUTPUT_LIMITER_STOCK));
  }
  for (int i = 0; i < 3; ++i) {
    s.set_output_limiter_mode(OutputLimiterMode(i));
    s.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.output_limiter_mode() == i);
    CHECK(loaded.state().media == 3);
  }
  // The legacy layout reads the new record with the same tag and size.
  LegacyState legacy_state = { };
  PersistentData legacy_calibration = { };
  stmlib::ChunkStorage<15, PersistentData, LegacyState> legacy;
  CHECK(legacy.Init(&legacy_calibration, &legacy_state));
  CHECK(legacy_state.media == 3 && legacy_state.padding[0] == 0);
  memset(legacy_state.padding, 0, sizeof(legacy_state.padding));
  legacy.SaveState();
  Settings downgrade;
  CHECK(downgrade.Init(false));
  CHECK(downgrade.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  puts("STAT persistence/compatibility: PASS");
}

// Frozen released limiter arithmetic, independent of ProcessOutput dispatch.
struct StockLimiter {
  float peak, lp, decay;
  explicit StockLimiter(float d) : peak(.5f), lp(.5f), decay(d) { }
  void Process(FloatFrame* x) {
    const float p = std::min(std::max(fabsf(x->l), fabsf(x->r)), 40.0f);
    SLOPE(peak, p, .1f, decay);
    SLOPE(lp, peak, .5f, decay);
    const float gain = lp <= .9f ? 1.0f : .9f / lp;
    x->l *= gain;
    x->r *= gain;
  }
};
void TestDsp() {
  test_checker.Start("TestDsp");
  const float sample_rates[] = { 24000.0f, 32000.0f, 48000.0f };
  for (size_t j = 0; j < 3; ++j) {
    const float sr = sample_rates[j];
    Limiter stock, fast, off;
    const float decay = 1.0f / (sr * 6.0f);
    stock.Init(decay, .9f);
    fast.Init(decay, .9f);
    off.Init(decay, .9f);
    StockLimiter reference(decay);
    for (int i = 0; i < 4096; ++i) {
      FloatFrame x = {sinf(i * .17f) * 4.0f, cosf(i * .13f) * 2.0f};
      FloatFrame y = x, z = x;
      stock.ProcessOutput(&y, 1, OUTPUT_LIMITER_STOCK, sr);
      reference.Process(&z);
      CHECK(!memcmp(&y, &z, sizeof(y)));
      off.ProcessOutput(&x, 1, OUTPUT_LIMITER_OFF, sr);
      CHECK(x.l == sinf(i * .17f) * 4.0f && x.r == cosf(i * .13f) * 2.0f);
      fast.ProcessOutput(&x, 1, OUTPUT_LIMITER_FAST, sr);
      CHECK(std::isfinite(x.l) && std::isfinite(x.r));
    }
    for (int i = 0; i < int(sr); ++i) {
      FloatFrame x = {.1f, -.1f}, y = x;
      stock.ProcessOutput(&x, 1, OUTPUT_LIMITER_STOCK, sr);
      fast.ProcessOutput(&y, 1, OUTPUT_LIMITER_FAST, sr);
      if (i == int(sr) - 1) {
        CHECK(y.l > x.l && y.l >= .099f);
      }
    }
  }
  puts("DSP mode dispatch: PASS");
}
void TestStockAssignRouting() {
  test_checker.Start("TestStockAssignRouting");
  Rig r;
  r.settings.mutable_state()->assignable_cv_destination = 0;
  for (int i = 1; i <= 6; ++i) {
    r.Tap();
    CHECK(r.settings.state().assignable_cv_destination == i % 3);
  }
  r.Tick(30);
  r.EnterLimiter();
  for (int i = 0; i < 4; ++i) {
    r.Tap();
    CHECK(r.settings.state().assignable_cv_destination == 0);
  }
}

void TestStockAssignAmounts() {
  test_checker.Start("TestStockAssignAmounts");
  for (int i = 0; i < 3; ++i) {
    Rig r;
    r.settings.set_stereo_width(128);
    r.settings.set_width_assigned(true);
    const State before = r.settings.state();
    uint32_t writes = test_writes;
    r.Down(SWITCH_ASSIGN);
    if (i == 0) {
      r.feedback = 0.25f;
    }
    if (i == 1) {
      r.pot = 0.75f;
    }
    if (i == 2) {
      r.reverb = 0.125f;
    }
    r.Tick(3100);
    CHECK(!r.ui.options_.active());
    const int expected[] = { 64, 192, 32 };
    CHECK(r.settings.state().assignable_cv_amount[i] == expected[i]);
    CHECK(r.settings.state().assignable_cv_destination == 3);
    r.Up(SWITCH_ASSIGN);
    CHECK(test_writes == writes + 1);
    Settings loaded;
    CHECK(loaded.Init(false));
    for (int j = 0; j < 3; ++j) {
      CHECK(loaded.state().assignable_cv_amount[j] ==
          (i == j ? expected[i] : before.assignable_cv_amount[j]));
    }
    CHECK(loaded.state().assignable_cv_destination == 3);
    CHECK(!loaded.width_assigned());
    CHECK(!r.ui.options_.active());
  }
}

void TestAutosaveChordAndDisplay() {
  test_checker.Start("TestAutosaveChordAndDisplay");
  Rig r;
  for (int i = 0; i < 2; ++i) {
    uint32_t writes = test_writes;
    const bool before = r.settings.state().disable_autosave;
    r.Down(SWITCH_ASSIGN);
    r.Down(SWITCH_FREEZE);
    r.Tick(200);
    CHECK(r.settings.state().disable_autosave == !before);
    CHECK(test_writes == writes + 1);
    CHECK(r.ui.leds_.intensity_[LED_FREEZE] == (!before ? 16 : 64));
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.state().disable_autosave == !before);
    r.Tick(4300);
    CHECK(r.settings.state().disable_autosave == !before);
    CHECK(test_writes == writes + 1);
    r.Up(SWITCH_FREEZE);
    r.Up(SWITCH_ASSIGN);
    CHECK(test_writes == writes + 1);
    CHECK(!r.ui.freeze_button_state_);
    r.Tick(1100);
  }
}

void TestSeedLatchGestures() {
  test_checker.Start("TestSeedLatchGestures");
  Rig r;
  r.Down(SWITCH_SEED);
  uint32_t start = r.ui.press_time_[SWITCH_SEED];
  while (stmlib::system_clock.milliseconds() - start < 3999) {
    r.Tick(1);
  }
  CHECK(!r.ui.seed_button_locked_);
  r.Tick(1);
  CHECK(r.ui.seed_button_locked_ && !r.ui.seed_button_state_);
  r.Up(SWITCH_SEED);
  CHECK(r.ui.seed_button_locked_);
  r.Down(SWITCH_SEED);
  r.Up(SWITCH_SEED);
  CHECK(!r.ui.seed_button_locked_ && !r.ui.seed_button_state_);
  r.Down(SWITCH_SEED);
  r.Down(SWITCH_FREEZE);
  CHECK(r.ui.seed_button_locked_ && !r.ui.seed_button_state_);
  CHECK(r.ui.ignore_release_[SWITCH_SEED]);
  CHECK(r.ui.ignore_release_[SWITCH_FREEZE]);
  r.Up(SWITCH_SEED);
  r.Up(SWITCH_FREEZE);
  CHECK(r.ui.seed_button_locked_ && !r.ui.freeze_button_state_);
}

void TestGainHoldThresholdAndReload() {
  test_checker.Start("TestGainHoldThresholdAndReload");
  Rig r;
  uint8_t media = r.settings.state().media;
  r.Down(SWITCH_MEDIA);
  CHECK(r.settings.state().media == media);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == (media + 3) % 4);
  r.settings.mutable_state()->gain = 123;
  r.settings.SaveState();
  uint32_t writes = test_writes;
  media = r.settings.state().media;
  r.Down(SWITCH_MEDIA);
  uint32_t start = r.ui.press_time_[SWITCH_MEDIA];
  while (stmlib::system_clock.milliseconds() - start < 1499) {
    r.Tick(1);
  }
  CHECK(r.settings.state().gain == 123 && test_writes == writes);
  r.Tick(1);
  CHECK(r.settings.state().gain == 0 && test_writes == writes + 1);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == media && test_writes == writes + 1);
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(loaded.state().gain == 0 && loaded.state().media == media);
}

void TestBackupLedAndRestore() {
  test_checker.Start("TestBackupLedAndRestore");
  Rig r;
  r.ui.freeze_led_state_ = true;
  r.backup.busy_ = true;
  r.Tick(1);
  CHECK(r.ui.leds_.intensity_[LED_FREEZE] == 128);
  r.backup.busy_ = false;
  r.Tick(1);
  CHECK(r.ui.leds_.intensity_[LED_FREEZE] == 255);
  r.Down(SWITCH_ASSIGN);
  r.Down(SWITCH_FREEZE);
  CHECK(r.ui.leds_.intensity_[LED_FREEZE] == 16);
  r.Up(SWITCH_FREEZE);
  r.Up(SWITCH_ASSIGN);
  r.Tick(1100);
  CHECK(r.ui.mode_ == Ui::MODE_NORMAL);
  CHECK(r.ui.leds_.intensity_[LED_FREEZE] == 255);
  const State& state = r.settings.state();
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1 +
      state.assignable_cv_destination] == 128);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] ==
      r.ui.media_palette_[state.media].r);
}

void TestLimiterLinkedGainAndModeSwitch() {
  test_checker.Start("TestLimiterLinkedGainAndModeSwitch");
  const OutputLimiterMode modes[] = {
    OUTPUT_LIMITER_STOCK, OUTPUT_LIMITER_FAST
  };
  for (int m = 0; m < 2; ++m) {
    Limiter limiter;
    Limiter fresh;
    limiter.Init(1.0f / (48000.0f * 6.0f), 0.9f);
    fresh.Init(1.0f / (48000.0f * 6.0f), 0.9f);
    FloatFrame block[32];
    for (int n = 0; n < 10; ++n) {
      for (int i = 0; i < 32; ++i) {
        block[i].l = 20.0f;
        block[i].r = -5.0f;
      }
      limiter.ProcessOutput(block, 32, modes[m], 48000.0f);
    }
    CHECK(fabsf(block[31].l) < 1.0f);
    for (int n = 0; n < 4; ++n) {
      for (int i = 0; i < 32; ++i) {
        block[i].l = 0.1f;
        block[i].r = -0.025f;
      }
      limiter.ProcessOutput(block, 32, OUTPUT_LIMITER_OFF, 48000.0f);
    }
    for (int n = 0; n < 4; ++n) {
      FloatFrame expected[32];
      for (int i = 0; i < 32; ++i) {
        block[i].l = expected[i].l = 0.1f;
        block[i].r = expected[i].r = -0.025f;
      }
      limiter.ProcessOutput(block, 32, modes[m], 48000.0f);
      fresh.ProcessOutput(expected, 32, modes[m], 48000.0f);
      for (int i = 0; i < 32; ++i) {
        CHECK(block[i].l == expected[i].l);
        CHECK(block[i].r == expected[i].r);
      }
    }
  }
  Limiter limiter;
  limiter.Init(1.0f / (48000.0f * 6.0f), 0.9f);
  for (int i = 0; i < 3000; ++i) {
    const OutputLimiterMode mode = OutputLimiterMode((i / 100) % 3);
    const float input = i % 2 ? 1.0e20f : -1.0e20f;
    FloatFrame x = { input, -input * 0.25f };
    limiter.ProcessOutput(&x, 1, mode, 48000.0f);
    CHECK(std::isfinite(x.l) && std::isfinite(x.r));
    // The detector cap does not bound the output to the audio range.
    CHECK(fabsf(x.l) > 1.0f && fabsf(x.l) <= fabsf(input));
    CHECK(x.r == -x.l * 0.25f);
    if (mode == OUTPUT_LIMITER_OFF) {
      CHECK(x.l == input);
    }
    FloatFrame small = { 0.1f, -0.025f };
    limiter.ProcessOutput(&small, 1, mode, 48000.0f);
    CHECK(std::isfinite(small.l) && std::isfinite(small.r));
    CHECK(fabsf(small.l) <= 0.1f && small.r == -small.l * 0.25f);
    if (mode == OUTPUT_LIMITER_OFF) {
      CHECK(small.l == 0.1f);
    }
  }
}

// Panel quality, gain and assignment displays.
void TestQualityTapCycle() {
  test_checker.Start("TestQualityTapCycle");
  Rig r;
  const int media[] = { 2, 1, 0, 3 };
  uint32_t writes = test_writes;
  for (int i = 0; i < 4; ++i) {
    r.Down(SWITCH_MEDIA);
    CHECK(r.settings.state().media == (i == 0 ? 3 : media[i - 1]));
    r.Up(SWITCH_MEDIA);
    CHECK(r.settings.state().media == media[i]);
    CHECK(test_writes == writes + i + 1);
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.state().media == media[i]);
  }
}

void TestManualGainGestureAndMeter() {
  test_checker.Start("TestManualGainGestureAndMeter");
  Rig r;
  const float positions[] = { 0.125f, 0.375f, 0.75f };
  const int gains[] = { 32, 96, 192 };
  const int red[] = { 0, 160, 255 };
  const int green[] = { 172, 255, 121 };
  const uint8_t media = r.settings.state().media;
  uint32_t writes = test_writes;
  r.Down(SWITCH_MEDIA);
  for (int i = 0; i < 3; ++i) {
    r.feedback = positions[i];
    r.Tick(20);
    CHECK(r.settings.state().gain == gains[i]);
    CHECK(r.ui.mix_parameter(Ui::MIX_PARAMETER_FEEDBACK) == 0.5f);
    CHECK(r.settings.state().media == media);
    CHECK(test_writes == writes);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == red[i]);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == green[i]);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == 0);
  }
  r.Tick(1700);
  CHECK(r.settings.state().gain == 192 && test_writes == writes);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == media);
  CHECK(test_writes == writes + 1);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == 192);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == 192);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == 208);
}

void TestSeedStartupLatch() {
  test_checker.Start("TestSeedStartupLatch");
  Rig r;
  CHECK(r.ui.seed_button_locked());
  CHECK(!r.ui.seed_button_state());
  CHECK(!r.ui.freeze_button_state());
}

void TestAssignableAmountLeds() {
  test_checker.Start("TestAssignableAmountLeds");
  Rig r;
  State* state = r.settings.mutable_state();
  state->assignable_cv_destination = 3;
  const uint8_t amounts[][3] = {
    { 0, 1, 255 }, { 255, 128, 64 }, { 32, 254, 129 }
  };
  for (int j = 0; j < 3; ++j) {
    memcpy(state->assignable_cv_amount, amounts[j], 3);
    r.Tick(1);
    for (int i = 0; i < 3; ++i) {
      CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1 + i] == amounts[j][i] / 2);
    }
  }
  for (int j = 0; j < 3; ++j) {
    state->assignable_cv_destination = j;
    r.Tick(1);
    for (int i = 0; i < 3; ++i) {
      CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1 + i] == (i == j ? 128 : 0));
    }
  }
}

void TestAgcAdjustmentBlink() {
  test_checker.Start("TestAgcAdjustmentBlink");
  Rig r;
  r.gain.adjusting_ = true;
  for (int i = 0; i < 256; ++i) {
    r.Tick(1);
    CHECK(r.ui.leds_.intensity_[LED_VU_METER] ==
        ((stmlib::system_clock.milliseconds() & 128) ? 128 : 0));
  }
  r.gain.adjusting_ = false;
  r.gain.meter_ = 73;
  r.Tick(1);
  CHECK(r.ui.leds_.intensity_[LED_VU_METER] == 73);
}

void TestRecoveryPreservesCalibration() {
  test_checker.Start("TestRecoveryPreservesCalibration");
  Rig r;
  const State defaults = r.settings.state();
  PersistentData* data = r.settings.mutable_persistent_data();
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    data->adc_calibration[i].scale = -3.0f - i;
    data->adc_calibration[i].offset = 0.25f + i;
  }
  r.settings.SavePersistentData();
  const PersistentData calibration = r.settings.persistent_data();
  memset(r.settings.mutable_state(), 1, sizeof(State));
  r.settings.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
  r.settings.SaveState();
  Settings recovered;
  CHECK(recovered.Init(true));
  CHECK(!memcmp(&recovered.state(), &defaults, sizeof(State)));
  CHECK(!memcmp(&recovered.persistent_data(), &calibration,
      sizeof(PersistentData)));
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(!memcmp(&loaded.state(), &defaults, sizeof(State)));
  CHECK(!memcmp(&loaded.persistent_data(), &calibration,
      sizeof(PersistentData)));
}

void TestAudioPatchDetection() {
  test_checker.Start("TestAudioPatchDetection");
  NormalizationDetector detector;
  detector.Init(2);
  uint16_t cv[CV_ADC_CHANNEL_LAST] = { };
  for (int patch = 0; patch < 4; ++patch) {
    for (int i = 0; i < 1400; ++i) {
      bool probe = detector.rng_state_ >> 31;
      Codec::Frame input;
      input.l = (patch & 1 ? !probe : probe) ? 1 : -1;
      input.r = (patch & 2 ? !probe : probe) ? 1 : -1;
      detector.Process(Codec::EVENT_NONE, input, cv);
    }
    CHECK(detector.normalized(NORMALIZATION_DETECTOR_CHANNEL_AUDIO_LEFT)
        == !(patch & 1));
    CHECK(detector.normalized(NORMALIZATION_DETECTOR_CHANNEL_AUDIO_RIGHT)
        == !(patch & 2));
    CHECK(detector.mono() == (patch != 3));
    CHECK(detector.no_input() == (patch == 0));
  }
}

void TestFrozenStateChangeGuard() {
  test_checker.Start("TestFrozenStateChangeGuard");
  Rig r;
  CHECK(!r.ui.should_ignore_state_change());
  r.Down(SWITCH_FREEZE);
  r.Up(SWITCH_FREEZE);
  CHECK(r.ui.freeze_button_state());
  r.ui.set_freeze_led_state(true);
  CHECK(r.ui.should_ignore_state_change());
  r.Down(SWITCH_MEDIA);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == 2);
  CHECK(r.ui.should_ignore_state_change());
  r.Down(SWITCH_FREEZE);
  r.Up(SWITCH_FREEZE);
  r.ui.set_freeze_led_state(false);
  CHECK(!r.ui.should_ignore_state_change());
  // The callback also merges the external FREEZE gate into this flag.
  r.ui.set_freeze_led_state(true);
  CHECK(r.ui.should_ignore_state_change());
  r.ui.set_freeze_led_state(false);
  CHECK(!r.ui.should_ignore_state_change());
}

void TestGateOutputChordPersistence() {
  test_checker.Start("TestGateOutputChordPersistence");
  Rig r;
  uint32_t writes = test_writes;
  const State before = r.settings.state();
  for (int i = 0; i < 2; ++i) {
    r.Down(SWITCH_ASSIGN);
    r.Down(SWITCH_SEED);
    CHECK(r.settings.state().gate_output_mode == !i);
    CHECK(test_writes == writes + i + 1);
    r.Tick(100);
    r.Up(SWITCH_SEED);
    r.Up(SWITCH_ASSIGN);
    CHECK(test_writes == writes + i + 1);
    CHECK(r.settings.state().assignable_cv_destination ==
        before.assignable_cv_destination);
    CHECK(r.ui.seed_button_locked());
    CHECK(!r.ui.seed_button_state());
    CHECK(!r.ui.options_.active());
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.state().gate_output_mode == !i);
  }
}

void TestOverwriteReserveDisplay() {
  test_checker.Start("TestOverwriteReserveDisplay");
  Rig r;
  const float reserve[] = { 1.0f, 0.5f, 0.0f, 0.5f, 1.0f };
  for (int media = 0; media < 4; ++media) {
    r.settings.mutable_state()->media = media;
    for (int i = 0; i < 5; ++i) {
      r.ui.set_overwrite_reserve(reserve[i]);
      r.Tick(1);
      Leds::Color color = r.ui.media_palette_[media];
      // LED conversion truncates; these binary fractions are exact.
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] ==
          uint8_t(color.r * reserve[i] * reserve[i]));
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] ==
          uint8_t(color.g * reserve[i]));
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] ==
          uint8_t(color.b * reserve[i]));
    }
  }
}

void TestCorruptCalibrationLocks() {
  test_checker.Start("TestCorruptCalibrationLocks");
  Rig r;
  const State defaults = r.settings.state();
  const PersistentData calibration = r.settings.persistent_data();
  r.settings.mutable_state()->media = 0;
  r.settings.SaveState();
  uint8_t* flash = reinterpret_cast<uint8_t*>(0x081e0000);
  flash[8] ^= 1;  // Corrupt CALI payload without changing its checksum.
  reinterpret_cast<uint8_t*>(0x081c0000)[8] ^= 1;  // And its mirror.
  static uint8_t sector[0x20000];
  memcpy(sector, flash, sizeof(sector));
  const uint32_t writes = test_writes;
  // Stock firmware rewrites the factory values and runs the factory test;
  // here every boot plays on the defaults and writes nothing.
  for (int i = 0; i < 3; ++i) {
    Settings next;
    CHECK(next.Init(false));
    CHECK(next.write_locked());
    CHECK(!memcmp(&next.state(), &defaults, sizeof(State)));
    CHECK(!memcmp(next.persistent_data().adc_calibration,
        calibration.adc_calibration, sizeof(calibration.adc_calibration)));
    next.mutable_state()->media = 1;
    next.SaveState();
    CHECK(test_writes == writes);
    CHECK(!memcmp(sector, flash, sizeof(sector)));
  }
}

void TestSelfPlayStorage() {
  test_checker.Start("TestSelfPlayStorage");
  Rig r;
  for (int i = 0; i < 256; ++i) {
    r.settings.mutable_state()->self_play = i;
    CHECK(r.settings.self_play_on_request() == (i == 0xa1));
    r.settings.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.self_play_on_request() == (i == 0xa1));
    CHECK(loaded.state().self_play == (i == 0xa1 ? 0xa1 : 0));
  }
  r.settings.set_self_play_on_request(true);
  r.settings.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
  r.settings.SaveState();
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.self_play_on_request());
  CHECK(loaded.output_limiter_mode() == OUTPUT_LIMITER_OFF);
  LegacyState legacy_state = { };
  PersistentData calibration = { };
  stmlib::ChunkStorage<15, PersistentData, LegacyState> legacy;
  CHECK(legacy.Init(&calibration, &legacy_state));
  memset(legacy_state.padding, 0, sizeof(legacy_state.padding));
  legacy.SaveState();
  CHECK(loaded.Init(false) && !loaded.self_play_on_request());
  loaded.mutable_state()->self_play = 0xff;
  loaded.stored_state_ = loaded.state_;
  loaded.stored_state_.padding[0] = 0;
  loaded.chunk_storage_.SaveState();
  CHECK(loaded.Init(false));
  CHECK(loaded.state().self_play == 0);
}

void TestAttackStorage() {
  test_checker.Start("TestAttackStorage");
  Rig r;
  for (int value = 0; value < 256; ++value) {
    r.settings.mutable_state()->attacks = value;
    const AttackMode mode = value == 0xa1 ? ATTACK_MODE_SNAP
        : value == 0xa2 ? ATTACK_MODE_SNAP_AND_TRIGGER : ATTACK_MODE_OFF;
    CHECK(r.settings.attack_mode() == mode);
    r.settings.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.attack_mode() == mode);
    CHECK(loaded.state().attacks == (mode ? 0xa0 + mode : 0));
    loaded.mutable_state()->attacks = value;
    loaded.stored_state_ = loaded.state_;
    loaded.stored_state_.padding[0] = 0;
    loaded.chunk_storage_.SaveState();
    CHECK(loaded.Init(false));
    CHECK(loaded.state().attacks == 0);
    CHECK(r.settings.Init(false));
  }
  // Page 1, REVERB, in thirds.
  // Far enough from the entry position (0.5) to take over.
  const float positions[] = { 0.05f, 0.62f, 0.95f };
  for (int zone = 0; zone < 3; ++zone) {
    Rig s;
    s.Enter();
    s.reverb = positions[zone];
    s.Tick(20);
    CHECK(s.ui.attack_mode() == AttackMode(zone));
    s.Tick(1100);
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.attack_mode() == AttackMode(zone));
  }
}

void TestFrozenFeedbackStorage() {
  test_checker.Start("TestFrozenFeedbackStorage");
  Rig r;
  for (int value = 0; value < 256; ++value) {
    r.settings.mutable_state()->frozen_feedback = value;
    CHECK(r.settings.evolving_feedback() == (value == 0xa1));
    r.settings.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.state().frozen_feedback == (value == 0xa1 ? 0xa1 : 0));
    loaded.mutable_state()->frozen_feedback = value;
    loaded.stored_state_ = loaded.state_;
    loaded.stored_state_.padding[0] = 0;
    loaded.chunk_storage_.SaveState();
    CHECK(loaded.Init(false));
    CHECK(!loaded.evolving_feedback());
    CHECK(r.settings.Init(false));
  }
  // Page 2, REVERB: upper half is Evolving, saved when the page settles.
  Rig s;
  s.Enter();
  s.Tap();
  s.reverb = 0.9f;
  s.Tick(20);
  CHECK(s.ui.evolving_feedback());
  s.Tick(1100);
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(loaded.evolving_feedback());
}

void TestCloudsStorage() {
  test_checker.Start("TestCloudsStorage");
  Rig r;
  for (int value = 0; value < 256; ++value) {
    r.settings.mutable_state()->reverb_style = value;
    CHECK(r.settings.clouds_style() == (value == 0xa2));
    r.settings.mutable_state()->pitch_scale = 0xa7;
    r.settings.mutable_state()->padding[0] = 0xff;
    r.settings.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.clouds_style() == (value == 0xa2));
    CHECK(loaded.state().reverb_style == (value == 0xa2 ? 0xa2 : 0));
    CHECK(loaded.state().pitch_scale == 0xa7);
    CHECK(loaded.pitch_scale() == PITCH_SCALE_PELOG);
    CHECK(loaded.state().padding[0] == 0);
    CHECK(r.settings.Init(false));
  }
}

void TestSynthEngineStorage() {
  test_checker.Start("TestSynthEngineStorage");
  Rig r;
  CHECK(!r.settings.plaits_synth() && !r.ui.plaits_synth());
  for (int value = 0; value < 256; ++value) {
    r.settings.mutable_state()->synth_engine = value;
    CHECK(r.settings.plaits_synth() == (value == 0xa1));
    r.settings.mutable_state()->random_intervals = 0xff;
    r.settings.mutable_state()->padding[0] = 0xff;
    r.settings.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.plaits_synth() == (value == 0xa1));
    CHECK(loaded.state().synth_engine == (value == 0xa1 ? 0xa1 : 0));
    CHECK(!loaded.state().random_intervals && !loaded.state().padding[0]);
    // Bypass SaveState normalization to exercise load normalization too.
    loaded.mutable_state()->synth_engine = value;
    loaded.stored_state_ = loaded.state_;
    loaded.stored_state_.padding[0] = 0;
    loaded.chunk_storage_.SaveState();
    CHECK(loaded.Init(false));
    CHECK(loaded.state().synth_engine == 0);
    CHECK(r.settings.Init(false));
  }
  // Page 3, DRY/WET knob: two halves with a dead band, previewed live.
  r.settings.set_plaits_synth(false);
  r.settings.SaveState();
  r.ui.LoadOptions();
  r.EnterPage(2);
  r.pot = .9f; r.Tick(1);
  CHECK(r.ui.options_.plaits_preview() && r.ui.plaits_synth());
  CHECK(!r.settings.plaits_synth());
  r.pot = .5f; r.Tick(1);
  CHECK(r.ui.options_.plaits_preview());
  r.pot = .46f; r.Tick(1);
  CHECK(!r.ui.options_.plaits_preview() && !r.ui.plaits_synth());
  r.pot = .54f; r.Tick(1001);
  CHECK(r.settings.plaits_synth());
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.plaits_synth());
  CHECK(loaded.state().synth_engine == 0xa1);
}

// Byte 20 holds Spectral and byte 11 the synth start; each is its own
// setting, and firmware without spectral reads byte 11 as before.
void TestEngineStorage() {
  test_checker.Start("TestEngineStorage");
  Rig r;
  for (int i = 0; i < 256; ++i) {
    r.settings.mutable_state()->engine = i;
    r.settings.mutable_state()->self_play = 0xa1;
    CHECK(r.settings.spectral() == (i == 0xa1));
    CHECK(r.settings.self_play_on_request());
    r.settings.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.spectral() == (i == 0xa1));
    CHECK(loaded.state().engine == (i == 0xa1 ? 0xa1 : 0));
    CHECK(loaded.self_play_on_request());
  }
  r.settings.set_spectral(true);
  r.settings.set_self_play_on_request(false);
  CHECK(r.settings.state().engine == 0xa1 && !r.settings.state().self_play);
  r.settings.set_self_play_on_request(true);
  CHECK(r.settings.state().engine == 0xa1);
  CHECK(r.settings.state().self_play == 0xa1);
  r.settings.set_spectral(false);
  CHECK(!r.settings.state().engine && r.settings.state().self_play == 0xa1);
  // Unknown legacy values read as Stock; the runtime view clears byte 23.
  r.settings.set_spectral(true);
  r.settings.mutable_state()->frozen_feedback = 0x12;
  r.settings.mutable_state()->attacks = 0x34;
  r.settings.mutable_state()->padding[0] = 0x56;
  r.settings.SaveState();
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.spectral());
  CHECK(!loaded.state().frozen_feedback && !loaded.state().attacks);
  CHECK(!loaded.state().padding[0]);
  CHECK(loaded.self_play_on_request());
  // A stock save clears the packed choices and selects Granular.
  LegacyState legacy_state = { };
  PersistentData calibration = { };
  stmlib::ChunkStorage<15, PersistentData, LegacyState> legacy;
  CHECK(legacy.Init(&calibration, &legacy_state));
  CHECK(legacy_state.padding[10] == 0x9f);
  memset(legacy_state.padding, 0, sizeof(legacy_state.padding));
  legacy.SaveState();
  CHECK(loaded.Init(false) && !loaded.spectral());
}

// Page 3, FEEDBACK: Granular / Spectral. The saved processor reaches the
// audio code only once the page has closed.
void TestEngineOption() {
  test_checker.Start("TestEngineOption");
  const float positions[2] = { 0.15f, 0.85f };
  for (int target = 0; target < 2; ++target) {
    Rig r;
    r.settings.set_spectral(!target);
    r.settings.set_self_play_on_request(true);
    r.ui.LoadOptions();
    const bool was_spectral = r.settings.spectral();
    r.EnterPage(2);
    CHECK(r.ui.options_.page() == 2);
    CHECK(r.ui.options_.spectral_preview() == was_spectral);
    r.feedback = target ? 0.05f : 0.95f;
    r.Tick(50);
    for (int step = 0; step < 20; ++step) {
      const float from = target ? 0.05f : 0.95f;
      r.feedback = from + (positions[target] - from) * (step + 1) / 20.0f;
      r.Tick(10);
    }
    CHECK(r.ui.options_.spectral_preview() == bool(target));
    const OptionValue& v = r.ui.options_.indication();
    CHECK(v.setting == OPTION_PROCESSOR && v.value == target);
    CHECK(v.color.r == (target ? 255 : 0) && v.color.g == 255);
    CHECK(v.color.b == 0 && v.pulses == PULSE_STEADY);
    r.Tick(1100);
    CHECK(r.settings.spectral() == bool(target));
    CHECK(r.settings.self_play_on_request());  // Synth start is separate.
    CHECK(r.ui.spectral() == was_spectral);
    r.Down(SWITCH_ASSIGN);
    r.Tick(3000);
    r.Up(SWITCH_ASSIGN);
    r.Tick(50);
    CHECK(r.ui.options_.stage() == Options::STAGE_INACTIVE);
    CHECK(r.ui.spectral() == bool(target));
    Settings loaded;
    CHECK(loaded.Init(false) && loaded.spectral() == bool(target));
    CHECK(loaded.self_play_on_request());
  }
}

// Byte 22: Random intervals. Unknown values read as Free (Stock).
void TestIntervalsStorage() {
  test_checker.Start("TestIntervalsStorage");
  Rig r;
  for (int value = 0; value < 256; ++value) {
    r.settings.mutable_state()->random_intervals = value;
    CHECK(r.settings.random_intervals() == (value == 0xa1));
    r.settings.mutable_state()->padding[0] = 0xff;
    r.settings.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.random_intervals() == (value == 0xa1));
    CHECK(loaded.state().random_intervals == (value == 0xa1 ? 0xa1 : 0));
    CHECK(loaded.state().padding[0] == 0);
    loaded.mutable_state()->random_intervals = value;
    loaded.stored_state_ = loaded.state_;
    loaded.stored_state_.padding[0] = 0;
    loaded.chunk_storage_.SaveState();
    CHECK(loaded.Init(false));
    CHECK(loaded.state().random_intervals == 0);
    CHECK(r.settings.Init(false));
  }
}

// Page 1, DRY/WET: Free / Fifths and octaves, independent of the scale.
void TestIntervalsOption() {
  test_checker.Start("TestIntervalsOption");
  for (int target = 0; target < 2; ++target) {
    Rig r;
    r.settings.set_pitch_scale(PITCH_SCALE_MAJOR);
    r.settings.set_random_intervals(!target);
    r.ui.LoadOptions();
    r.Enter();
    CHECK(r.ui.options_.intervals_preview() == !target);
    r.pot = target ? 0.05f : 0.95f;
    r.Tick(50);
    r.pot = target ? 0.85f : 0.15f;
    r.Tick(50);
    CHECK(r.ui.options_.intervals_preview() == bool(target));
    CHECK(r.ui.random_intervals() == bool(target));
    const OptionValue& v = r.ui.options_.indication();
    CHECK(v.setting == OPTION_INTERVALS && v.value == target);
    CHECK(v.color.r == (target ? 255 : 0) && v.color.g == 255);
    r.Tick(1100);
    CHECK(r.settings.random_intervals() == bool(target));
    CHECK(r.settings.pitch_scale() == PITCH_SCALE_MAJOR);
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.random_intervals() == bool(target));
    CHECK(loaded.state().random_intervals == (target ? 0xa1 : 0));
  }
}

// Page 3, REVERB: Automatic / Manual synth start, independent of the
// processor.
void TestSynthStartOption() {
  test_checker.Start("TestSynthStartOption");
  for (int target = 0; target < 2; ++target) {
    Rig r;
    r.settings.set_spectral(true);
    r.settings.set_self_play_on_request(!target);
    r.ui.LoadOptions();
    r.EnterPage(2);
    CHECK(r.ui.options_.manual_start_preview() == !target);
    r.reverb = target ? 0.05f : 0.95f;
    r.Tick(50);
    r.reverb = target ? 0.85f : 0.15f;
    r.Tick(50);
    CHECK(r.ui.options_.manual_start_preview() == bool(target));
    const OptionValue& v = r.ui.options_.indication();
    CHECK(v.setting == OPTION_SYNTH_START && v.value == target);
    r.Tick(1100);
    CHECK(r.settings.self_play_on_request() == bool(target));
    CHECK(r.settings.spectral());
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.self_play_on_request() == bool(target));
    CHECK(loaded.state().self_play == (target ? 0xa1 : 0));
  }
}

void TestSelfPlayHold() {
  test_checker.Start("TestSelfPlayHold");
  for (int mode = 0; mode < 2; ++mode) {
    for (int no_input = 0; no_input < 2; ++no_input) {
      for (int frozen = 0; frozen < 2; ++frozen) {
        Rig r;
        r.settings.set_self_play_on_request(mode);
        r.ui.set_no_input(no_input);
        r.ui.freeze_led_state_ = frozen;
        r.settings.mutable_state()->gain = 123;
        uint32_t writes = test_writes;
        uint8_t media = r.settings.state().media;
        // While frozen, the hold restarts automatic gain as in stock.
        const bool request = mode && no_input && !frozen;
        for (int hold = 0; hold < 2; ++hold) {
          r.Down(SWITCH_MEDIA);
          uint32_t start = r.ui.press_time_[SWITCH_MEDIA];
          while (stmlib::system_clock.milliseconds() - start < 1499) {
            r.Tick(1);
          }
          CHECK(!r.ui.self_play_requested());
          r.Tick(1);
          CHECK(r.ui.self_play_requested() == request);
          CHECK(r.gain.restarts == (request ? 0 : hold + 1));
          if (request) {
            CHECK(r.settings.state().gain == 123 && test_writes == writes);
          }
          r.ui.ClearSelfPlayRequest();
          CHECK(!r.ui.self_play_requested());
          r.Up(SWITCH_MEDIA);
          CHECK(!r.ui.self_play_requested());
          CHECK(r.settings.state().media == media);
        }
      }
    }
  }
}

// Settled option saves until the sector fills; a power cut anywhere in the
// rollover that erases and rewrites calibration and state together.
const uint32_t kCalibrationChunk = (8 + sizeof(PersistentData) + 31) & ~31u;
const uint32_t kStateChunk = (8 + sizeof(State) + 31) & ~31u;
const int kStateChunks = (0x20000 - kCalibrationChunk) / kStateChunk;

void TestSettingsRollover() {
  test_checker.Start("TestSettingsRollover");
  // Rolling save: erase, four calibration blocks, one state block.
  for (int cut = 0; cut <= 6; ++cut) {
    Rig r;
    const State defaults = r.settings.state();
    const PersistentData factory = r.settings.persistent_data();
    PersistentData* calibration = r.settings.mutable_persistent_data();
    for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
      calibration->adc_calibration[i].scale = -2.5f - 0.125f * i;
      calibration->adc_calibration[i].offset = 0.75f + 0.0625f * i;
    }
    r.settings.SavePersistentData();
    r.settings.set_output_limiter_mode(OUTPUT_LIMITER_FAST);
    r.settings.set_self_play_on_request(true);
    r.settings.set_pitch_scale(PITCH_SCALE_PENTATONIC_MAJOR);
    r.settings.set_clouds_style(false);
    r.settings.set_stereo_width(200);
    r.settings.set_width_assigned(true);
    r.settings.set_delay_mode(DELAY_MODE_WIDE_DETENT);
    r.settings.SaveState();
    const PersistentData old_calibration = r.settings.persistent_data();
    r.EnterPage(1);
    CHECK(!r.ui.options_.clouds_preview());
    // Real settled saves: the DRY/WET knob alone flips the style each time.
    const uint32_t erases = TestFlashErases();
    for (int i = 0; i < 2; ++i) {
      const uint32_t writes = test_writes;
      r.pot = i ? .1f : .9f;
      r.Tick(1100);
      CHECK(test_writes == writes + 1);
    }
    CHECK(!r.settings.clouds_style());
    while (r.settings.chunk_storage_.next_state_chunk_index_ <=
        kStateChunks) {
      r.settings.SaveState();
    }
    CHECK(TestFlashErases() == erases);
    const State old_state = r.settings.state();
    State new_state = old_state;
    new_state.reverb_style = kOptionTag + 2;

    const uint32_t writes = test_writes;
    TestFlashBudget() = cut;
    r.pot = .9f;
    r.Tick(1100);
    TestFlashBudget() = -1;
    CHECK(r.settings.clouds_style());
    CHECK(TestFlashErases() == erases + (cut ? 1 : 0));
    CHECK(test_writes == writes + (cut > 1 ? cut - 1 : 0));

    Settings loaded;
    // Never the factory test.
    CHECK(loaded.Init(false));
    const State& state = loaded.state();
    const AdcCalibrationData* data = loaded.persistent_data().adc_calibration;
    if (cut == 0) {
      // Power went before the erase: the previous record is untouched.
      CHECK(!loaded.write_locked());
      CHECK(!memcmp(&state, &old_state, sizeof(State)));
      CHECK(!memcmp(data, old_calibration.adc_calibration,
          sizeof(old_calibration.adc_calibration)));
    } else if (cut == 1 || cut == 5) {
      // Erased, or the calibration written but no state: the calibration
      // survives (from the mirror after the erase), the options reset.
      CHECK(!loaded.write_locked());
      CHECK(!memcmp(&state, &defaults, sizeof(State)));
      CHECK(!memcmp(data, old_calibration.adc_calibration,
          sizeof(old_calibration.adc_calibration)));
    } else if (cut < 5) {
      // A partly written calibration: damaged, so the defaults play from
      // RAM until SEED at power-up repairs the sector. The mirrored
      // calibration is more than 25% from the factory values here, so the
      // factory values play.
      CHECK(loaded.write_locked());
      CHECK(!memcmp(&state, &defaults, sizeof(State)));
      CHECK(!memcmp(data, factory.adc_calibration,
          sizeof(factory.adc_calibration)));
    } else {
      CHECK(!loaded.write_locked());
      CHECK(!memcmp(&state, &new_state, sizeof(State)));
      CHECK(!memcmp(data, old_calibration.adc_calibration,
          sizeof(old_calibration.adc_calibration)));
      CHECK(loaded.chunk_storage_.next_state_chunk_index_ == 2);
      CHECK(loaded.output_limiter_mode() == OUTPUT_LIMITER_FAST);
      CHECK(loaded.self_play_on_request());
      CHECK(loaded.pitch_scale() == PITCH_SCALE_PENTATONIC_MAJOR);
      CHECK(loaded.clouds_style());
      CHECK(loaded.stereo_width() == 200);
      CHECK(loaded.width_assigned());
      CHECK(loaded.delay_mode() == DELAY_MODE_WIDE_DETENT);
    }
    // Whatever came back is stable.
    Settings again;
    CHECK(again.Init(false));
    CHECK(again.write_locked() == loaded.write_locked());
    CHECK(!memcmp(&again.state(), &state, sizeof(State)));
    CHECK(!memcmp(again.persistent_data().adc_calibration, data,
        sizeof(old_calibration.adc_calibration)));
  }
}

// Settings saves until the next one rewrites the sector.
void FillSettingsSector(Settings* settings) {
  while (settings->chunk_storage_.next_state_chunk_index_ <= kStateChunks) {
    settings->SaveState();
  }
}

bool SettingsLoad(const State& state, const PersistentData& calibration) {
  Settings loaded;
  return loaded.Init(false) && \
      !memcmp(&loaded.state(), &state, sizeof(State)) && \
      !memcmp(loaded.persistent_data().adc_calibration,
          calibration.adc_calibration, sizeof(calibration.adc_calibration));
}

const uint32_t kStaleFlag = 0x00040000;  // PGSERR

void TestStorageStaleErrorFlag() {
  test_checker.Start("TestStorageStaleErrorFlag");
  // An error flag left by an earlier operation: stmlib's rewrite skips the
  // erase and programs the records over the old ones. (The masked reader
  // clears the ECC flags its own reads raise.)
  {
    Rig r;
    FillSettingsSector(&r.settings);
    PersistentData calibration = r.settings.persistent_data();
    State state = r.settings.state();
    stmlib::ChunkStorage<15, PersistentData, State> stock;
    stock.Init(&calibration, &state);
    const uint32_t overwrites = TestFlashOverwrites();
    TestFlashErrors() = kStaleFlag;
    stock.SavePersistentData();
    CHECK(TestFlashOverwrites() > overwrites);
  }
  // The Beads storage clears the flag first: one erase, no overwrite.
  Rig r;
  FillSettingsSector(&r.settings);
  r.settings.mutable_state()->media = 1;
  const uint32_t overwrites = TestFlashOverwrites();
  const uint32_t erases = TestFlashErases();
  const uint32_t refusals = TestFlashRefusals();
  TestFlashErrors() = kStaleFlag;
  r.settings.SaveState();
  CHECK(TestFlashErrors() == 0);
  CHECK(TestFlashRefusals() == refusals);  // Cleared before the erase.
  CHECK(TestFlashOverwrites() == overwrites);
  CHECK(TestFlashErases() == erases + 1);
  CHECK(r.settings.chunk_storage_.next_state_chunk_index_ == 2);
  CHECK(SettingsLoad(r.settings.state(), r.settings.persistent_data()));
  // The same on an ordinary save, which appends a record.
  r.settings.mutable_state()->media = 2;
  TestFlashErrors() = kStaleFlag;
  r.settings.SaveState();
  CHECK(TestFlashRefusals() == refusals);  // Cleared before the program.
  CHECK(TestFlashOverwrites() == overwrites);
  CHECK(r.settings.chunk_storage_.next_state_chunk_index_ == 3);
  CHECK(SettingsLoad(r.settings.state(), r.settings.persistent_data()));
}

void TestStorageFailedErase() {
  test_checker.Start("TestStorageFailedErase");
  for (int stuck = 0; stuck < 2; ++stuck) {
    Rig r;
    FillSettingsSector(&r.settings);
    const State old_state = r.settings.state();
    const PersistentData calibration = r.settings.persistent_data();
    r.settings.mutable_state()->media = 1;
    const uint32_t overwrites = TestFlashOverwrites();
    // Every attempt refused, or reported done with the sector unchanged.
    if (stuck) {
      TestFlashStuckErases() = 100;
    } else {
      TestFlashFailures() = 100;
    }
    r.settings.SaveState();
    TestFlashStuckErases() = 0;
    TestFlashFailures() = 0;
    // Nothing programmed over the old records, which still load.
    CHECK(TestFlashOverwrites() == overwrites);
    CHECK(SettingsLoad(old_state, calibration));
    // The settings stay in RAM and the next save rewrites the sector.
    r.settings.SaveState();
    CHECK(TestFlashOverwrites() == overwrites);
    CHECK(r.settings.chunk_storage_.next_state_chunk_index_ == 2);
    CHECK(SettingsLoad(r.settings.state(), calibration));
  }
}

void TestStorageNonBlankSlot() {
  test_checker.Start("TestStorageNonBlankSlot");
  // A record is never programmed over a free chunk that isn't blank.
  Rig r;
  r.settings.SaveState();
  const uint16_t index = r.settings.chunk_storage_.next_state_chunk_index_;
  uint8_t* slot = reinterpret_cast<uint8_t*>(
      0x081e0000 + kCalibrationChunk + kStateChunk * (index - 1));
  slot[5] = 0;
  r.settings.mutable_state()->media = 1;
  const uint32_t overwrites = TestFlashOverwrites();
  r.settings.SaveState();
  CHECK(TestFlashOverwrites() == overwrites);
  CHECK(r.settings.chunk_storage_.next_state_chunk_index_ == 2);
  CHECK(SettingsLoad(r.settings.state(), r.settings.persistent_data()));
}

void TestStorageFailedProgram() {
  test_checker.Start("TestStorageFailedProgram");
  for (int failures = 1; failures <= 100; failures += 99) {
    Rig r;
    r.settings.mutable_state()->media = 1;
    r.settings.SaveState();
    const State old_state = r.settings.state();
    const PersistentData calibration = r.settings.persistent_data();
    const uint16_t index = r.settings.chunk_storage_.next_state_chunk_index_;
    r.settings.mutable_state()->media = 2;
    const uint32_t overwrites = TestFlashOverwrites();
    TestFlashFailures() = failures;
    r.settings.SaveState();
    TestFlashFailures() = 0;
    CHECK(TestFlashOverwrites() == overwrites);
    if (failures == 1) {
      // A refused program is retried in place.
      CHECK(r.settings.chunk_storage_.next_state_chunk_index_ == index + 1);
      CHECK(SettingsLoad(r.settings.state(), calibration));
    } else {
      // Refused every time: the previous record still loads.
      CHECK(SettingsLoad(old_state, calibration));
      r.settings.SaveState();
      CHECK(TestFlashOverwrites() == overwrites);
      CHECK(r.settings.chunk_storage_.next_state_chunk_index_ == 2);
      CHECK(SettingsLoad(r.settings.state(), calibration));
    }
  }
}

void TestStorageMatchesStmlib() {
  test_checker.Start("TestStorageMatchesStmlib");
  // Normal saves leave the sector byte for byte as stmlib's storage does.
  static uint8_t ours[0x20000];
  uint8_t* sector = reinterpret_cast<uint8_t*>(0x081e0000);
  for (int pass = 0; pass < 2; ++pass) {
    memset(reinterpret_cast<void*>(0x081c0000), 255, 0x40000);
    PersistentData calibration;
    State state;
    memset(&calibration, 0, sizeof(calibration));
    memset(&state, 0, sizeof(state));
    for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
      calibration.adc_calibration[i].scale = -2.5f - 0.125f * i;
      calibration.adc_calibration[i].offset = 0.75f + 0.0625f * i;
    }
    if (pass == 0) {
      ChunkStorage<15, 14, PersistentData, State> storage;
      storage.Init(&calibration, &state);
      for (int i = 0; i < kStateChunks + 10; ++i) {
        state.media = i & 3;
        state.gain = uint8_t(i);
        storage.SaveState();
      }
      storage.SavePersistentData();
      memcpy(ours, sector, sizeof(ours));
    } else {
      stmlib::ChunkStorage<15, PersistentData, State> storage;
      storage.Init(&calibration, &state);
      for (int i = 0; i < kStateChunks + 10; ++i) {
        state.media = i & 3;
        state.gain = uint8_t(i);
        storage.SaveState();
      }
      storage.SavePersistentData();
      CHECK(!memcmp(ours, sector, sizeof(ours)));
    }
  }
}

void TestPitchStorage() {
  test_checker.Start("TestPitchStorage");
  Rig r;
  r.settings.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
  r.settings.set_self_play_on_request(true);
  for (int i = 0; i < 256; ++i) {
    const PitchScale expected = i >= 0xa1 && i <= 0xa9
        ? PitchScale(i - 0xa0) : PITCH_SCALE_OFF;
    r.settings.mutable_state()->pitch_scale = i;
    CHECK(r.settings.pitch_scale() == expected);
    r.settings.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.pitch_scale() == expected);
    CHECK(loaded.state().pitch_scale == (expected ? i : 0));
    loaded.mutable_state()->padding[0] = 0xff;
    loaded.SaveState();
    CHECK(r.settings.Init(false));
    CHECK(r.settings.pitch_scale() == expected);
    CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_OFF);
    CHECK(r.settings.self_play_on_request());
    CHECK(r.settings.state().padding[0] == 0);
  }
}

void TestPlaybackStorage() {
  test_checker.Start("TestPlaybackStorage");
  for (int raw = 0; raw < 256; ++raw) {
    Rig r;
    r.settings.set_stereo_width(128);
    r.settings.mutable_state()->width_assigned = raw;
    CHECK(r.settings.width_assigned() == (raw == 1));
  }

  Rig r;
  for (int width = 0; width < 256; ++width) {
    for (int delay = 0; delay < 3; ++delay) {
      r.settings.set_pitch_scale(PITCH_SCALE_SHRI);
      r.settings.set_clouds_style(true);
      r.settings.set_stereo_width(StereoWidth(width));
      r.settings.set_width_assigned(width != 0);
      r.settings.set_delay_mode(DelayMode(delay));
      r.settings.SaveState();
      Settings loaded;
      loaded.Init(false);
      CHECK(loaded.stereo_width() == StereoWidth(width));
      CHECK(loaded.state().assignable_cv_destination == 0);
      CHECK(loaded.width_assigned() == (width != 0));
      CHECK(loaded.state().width_assigned == (width ? 1 : 0));
      State stock_view = loaded.state();
      CONSTRAIN(stock_view.assignable_cv_destination, 0, 3);
      CHECK(stock_view.assignable_cv_destination == 0);
      CHECK(loaded.delay_mode() == DelayMode(delay));
      CHECK(loaded.state().pitch_scale == 0xa9);
      CHECK(loaded.state().reverb_style == 0xa2);
      CHECK(loaded.state().stereo_width == width);
      CHECK(loaded.state().delay_mode == (delay ? 0xa0 + delay : 0));
      for (int byte = 7; byte < 14; ++byte) {
        CHECK(reinterpret_cast<const uint8_t*>(
            &loaded.state())[byte + 10] == 0);
      }
    }
  }
}

void TestWidthAssignCycle() {
  test_checker.Start("TestWidthAssignCycle");
  Rig r;
  r.settings.mutable_state()->assignable_cv_destination = 3;
  r.Down(SWITCH_ASSIGN);
  r.Up(SWITCH_ASSIGN);
  CHECK(r.settings.state().assignable_cv_destination == 0);
  for (int enabled = 0; enabled < 2; ++enabled) {
    r.settings.set_stereo_width(enabled ? 128 : 0);
    r.settings.mutable_state()->assignable_cv_destination = 0;
    r.Tick(20);
    const int expected[] = { 1, 2, 4, 0 };
    for (int i = 0; i < (enabled ? 4 : 3); ++i) {
      r.Down(SWITCH_ASSIGN);
      r.Up(SWITCH_ASSIGN);
      CHECK(r.settings.state().assignable_cv_destination <= 3);
      CHECK((r.settings.width_assigned() ? 4 :
          r.settings.state().assignable_cv_destination) ==
          (enabled ? expected[i] : (i + 1) % 3));
    }
  }
}

void TestDiffusionGestureAndMeter() {
  test_checker.Start("TestDiffusionGestureAndMeter");
  Rig r;
  const uint8_t media = r.settings.state().media;
  const uint8_t gain = r.settings.state().gain;
  CHECK(r.settings.state().diffusion == 0);
  CHECK(r.settings.diffusion() == 0.0f);
  uint32_t writes = test_writes;
  r.Down(SWITCH_MEDIA);
  const float positions[] = { 0.125f, 0.75f, 0.0f };
  for (int i = 0; i < 3; ++i) {
    r.reverb = positions[i];
    r.Tick(20);
    CHECK(r.settings.state().diffusion ==
        1 + int(positions[i] * 254.0f + 0.5f));
    CHECK(fabsf(r.settings.diffusion() - positions[i]) < 0.003f);
    CHECK(r.ui.mix_parameter(Ui::MIX_PARAMETER_REVERB) == 0.5f);
    CHECK(r.settings.state().gain == gain);
    CHECK(test_writes == writes);
  }
  r.reverb = 0.5f;
  r.Tick(20);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == 0);  // The meter, not quality.
  r.Tick(1700);  // Holding while turning never restarts the gain.
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == media);
  CHECK(r.settings.state().gain == gain);
  CHECK(test_writes == writes + 1);
  Settings loaded;
  CHECK(loaded.Init(false));
  CHECK(loaded.state().diffusion == 128);
  // A quality tap without turning REVERB leaves the amount alone.
  r.Down(SWITCH_MEDIA);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == (media + 3) % 4);
  CHECK(r.settings.state().diffusion == 128);
}

void TestDiffusionStorage() {
  test_checker.Start("TestDiffusionStorage");
  for (int clouds = 0; clouds < 2; ++clouds) {
    for (int raw = 0; raw < 256; ++raw) {
      Rig r;
      r.settings.set_clouds_style(clouds);
      r.settings.mutable_state()->diffusion = raw;
      const float expected = raw
          ? float(raw - 1) / 254.0f : (clouds ? kCloudsDiffusion : 0.0f);
      CHECK(r.settings.diffusion() == expected);
      r.settings.mutable_state()->assignable_cv_destination = 2;
      r.settings.set_diffusion_assigned(true);
      // Clouds-style's own amount is not a CV destination until set.
      const bool assignable = raw > 1;
      CHECK(r.settings.diffusion_assigned() == assignable);
      r.settings.SaveState();
      Settings loaded;
      CHECK(loaded.Init(false));
      CHECK(loaded.state().diffusion == raw);
      CHECK(loaded.diffusion_assigned() == assignable);
      CHECK(!loaded.width_assigned());
      CHECK(loaded.state().assignable_cv_destination == (assignable ? 0 : 2));
    }
  }
  Rig r;
  r.settings.set_diffusion(0.5f);
  r.settings.set_stereo_width(128);
  r.settings.set_diffusion_assigned(true);
  r.settings.set_stereo_width(0);  // Width off keeps the diffusion target.
  CHECK(r.settings.diffusion_assigned());
  r.settings.set_diffusion(0.0f);  // No diffusion left to modulate.
  CHECK(!r.settings.diffusion_assigned());
  CHECK(r.settings.state().diffusion == 1);
  r.settings.set_diffusion(1.02f);  // A pot reading past the end.
  CHECK(r.settings.state().diffusion == 255);
}

void TestDiffusionAssignCycle() {
  test_checker.Start("TestDiffusionAssignCycle");
  // Destinations: 0-2 mix knobs, 4 WIDTH, 5 DIFFUSION.
  const int cycles[4][6] = {
    { 1, 2, 0, 1, 2, 0 },
    { 1, 2, 4, 0, 1, 2 },
    { 1, 2, 5, 0, 1, 2 },
    { 1, 2, 4, 5, 0, 1 },
  };
  for (int c = 0; c < 4; ++c) {
    Rig r;
    r.settings.set_stereo_width(c & 1 ? 128 : 0);
    if (c & 2) r.settings.set_diffusion(0.4f);
    r.settings.mutable_state()->assignable_cv_destination = 0;
    r.Tick(20);
    for (int i = 0; i < 6; ++i) {
      r.Down(SWITCH_ASSIGN);
      r.Up(SWITCH_ASSIGN);
      const int destination = r.settings.width_assigned() ? 4
          : r.settings.diffusion_assigned() ? 5
          : r.settings.state().assignable_cv_destination;
      CHECK(destination == cycles[c][i]);
      if (destination == 5) {
        for (int led = 0; led < 3; ++led) {
          CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1 + led] == 0 ||
              led == Ui::MIX_PARAMETER_REVERB);
        }
      }
    }
  }
  // Clouds-style alone keeps the Beta 1 cycle.
  {
    Rig r;
    r.settings.set_clouds_style(true);
    r.settings.mutable_state()->assignable_cv_destination = 2;
    r.Tap();
    CHECK(!r.settings.diffusion_assigned());
    CHECK(r.settings.state().assignable_cv_destination == 0);
  }
  // [M]+knob amounts replace the DIFFUSION target.
  Rig r;
  r.settings.set_diffusion(0.4f);
  r.settings.mutable_state()->assignable_cv_destination = 2;
  r.Tap();
  CHECK(r.settings.diffusion_assigned());
  r.Down(SWITCH_ASSIGN);
  r.feedback = 0.9f;
  r.Tick(20);
  r.Up(SWITCH_ASSIGN);
  CHECK(!r.settings.diffusion_assigned());
  CHECK(r.settings.state().assignable_cv_destination == 3);
}

// [A]+DRY/WET sets the stereo width as Beads' [A]+FEEDBACK sets the gain:
// fully CCW is stock, and the amount saves when [A] is released.
void TestWidthGestureAndMeter() {
  test_checker.Start("TestWidthGestureAndMeter");
  Rig r;
  const uint8_t media = r.settings.state().media;
  r.settings.set_stereo_width(128);
  r.settings.mutable_state()->assignable_cv_destination = 2;
  r.settings.set_width_assigned(true);
  const uint32_t writes = test_writes;
  r.Down(SWITCH_MEDIA);
  r.pot = .9f;
  r.Tick(20);
  CHECK(r.settings.stereo_width() ==
      1 + int((.9f - .13f) / .87f * 254.0f + .5f));
  CHECK(r.ui.mix_parameter(Ui::MIX_PARAMETER_DRY_WET) == 0.5f);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == 0);  // The meter, not quality.
  // Every code on Beads' meter, below its wrap at 1.0.
  for (int width = 0; width <= 255; ++width) {
    r.settings.mutable_state()->stereo_width = width;
    r.ui.UpdateLeds();
    Leds meter;
    meter.Init();
    meter.set_meter(width ? .25f + .749f * float(width - 1) / 254.0f : .05f);
    for (int channel = LED_MEDIA_R; channel <= LED_MEDIA_B; ++channel) {
      CHECK(r.ui.leds_.intensity_[channel] == meter.intensity_[channel]);
    }
  }
  r.pot = .1f;  // Inside the dead band: still on.
  r.Tick(20);
  CHECK(r.settings.stereo_width() == 1);
  CHECK(r.settings.width_assigned());
  r.pot = .06f;
  r.Tick(20);
  CHECK(r.settings.stereo_width() == 0);
  CHECK(!r.settings.width_assigned());
  r.pot = .1f;  // Off stays off inside the dead band.
  r.Tick(20);
  CHECK(r.settings.stereo_width() == 0);
  r.pot = .5f;
  r.Tick(1700);  // Holding while turning never restarts the gain.
  CHECK(test_writes == writes);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == media);
  CHECK(r.gain.restarts == 0);
  CHECK(test_writes == writes + 1);
  const StereoWidth width = r.settings.stereo_width();
  CHECK(width > 1);
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.stereo_width() == width);
  // A quality tap without turning DRY/WET leaves the width alone.
  r.Down(SWITCH_MEDIA);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.state().media == (media + 3) % 4);
  CHECK(r.settings.stereo_width() == width);
  // The options page no longer holds the width.
  r.Tick(30);
  r.Enter();
  for (int page = 0; page < kNumOptionPages; ++page) {
    for (int knob = 0; knob < 3; ++knob) {
      CHECK(kOptionPages[page].knobs[knob] != OPTION_RESERVED ||
          page == 0 || page == 3);
    }
  }
  r.pot = .02f;
  r.Tick(1100);
  CHECK(r.settings.stereo_width() == width);
}

void TestOptionsTablesAndLifetime() {
  test_checker.Start("TestOptionsTablesAndLifetime");
  const OptionSetting expected[4][3] = {
    { OPTION_PITCH, OPTION_INTERVALS, OPTION_ATTACKS },
    { OPTION_DELAY, OPTION_REVERB, OPTION_FROZEN },
    { OPTION_PROCESSOR, OPTION_SYNTH, OPTION_SYNTH_START },
    { OPTION_RESERVED, OPTION_RESERVED, OPTION_LIMITER }
  };
  CHECK(kNumOptionPages == 4);
  Rig r;
  r.Enter();
  r.Tick(100);  // Entry confirmation has ended.
  for (int page = 0; page < kNumOptionPages; ++page) {
    CHECK(r.ui.options_.page() == page);
    // Left to right; the last page lights all three.
    CHECK(kOptionPages[page].led_index == page);
    const int preview = page == 3 ? 2 : 0;
    CHECK(r.ui.options_.preview_knob() == preview);
    for (int knob = 0; knob < 3; ++knob) {
      CHECK(kOptionPages[page].knobs[knob] == expected[page][knob]);
      const int led = LED_ASSIGN_1 + knob;
      if (knob == preview) {
        CHECK(r.ui.leds_.intensity_[led] == 255);
      } else if (page != 3 && knob != page) {
        CHECK(r.ui.leds_.intensity_[led] == 0);
      }
    }
    r.Tap();
  }
  CHECK(r.ui.options_.page() == 0);
  r.Tap();
  r.Tick(60000);
  CHECK(r.ui.options_.stage() == Options::STAGE_EDITING);
  r.Down(SWITCH_MEDIA);
  r.Up(SWITCH_MEDIA);
  r.Tick(30);
  r.Enter();
  CHECK(r.ui.options_.page() == 1);
  r.Down(SWITCH_ASSIGN);
  r.Tick(3000);
  r.Up(SWITCH_ASSIGN);
  CHECK(!r.ui.options_.active());
  r.Tick(30);
  r.Enter();
  CHECK(r.ui.options_.page() == 1);
  Rig startup(true);
  startup.Tick(3500);
  CHECK(!startup.ui.options_.active());
}

void TestOptionsKnobsAndSettling() {
  test_checker.Start("TestOptionsKnobsAndSettling");
  // Every physical knob.
  for (int page = 0; page < kNumOptionPages; ++page) {
    for (int knob = 0; knob < 3; ++knob) {
      Rig r;
      r.Enter();
      for (int i = 0; i < page; ++i) r.Tap();
      float* pot = knob == 0 ? &r.feedback : knob == 1 ? &r.pot : &r.reverb;
      const OptionSetting setting = kOptionPages[page].knobs[knob];
      const State state = r.settings.state();
      const uint32_t writes = test_writes;
      *pot = 0.59f;
      r.Tick(1500);
      CHECK(!r.ui.options_.qualified_[knob]);
      CHECK(test_writes == writes);
      *pot = 0.9f;
      r.Tick(100);
      if (setting == OPTION_RESERVED) {
        CHECK(!r.ui.options_.qualified_[knob]);
        CHECK(!memcmp(&state, &r.settings.state(), sizeof(state)));
        r.Tick(2000);
        CHECK(test_writes == writes);
        continue;
      }
      CHECK(r.ui.options_.selected_ == setting);
      CHECK(r.ui.options_.qualified_[knob]);
      for (int step = 0; step < 20; ++step) {
        *pot = 0.95f - 0.045f * step;
        r.Tick(100);
      }
      *pot = 0.9f;
      r.Tick(999);
      CHECK(test_writes == writes);
      r.Tick(2);
      CHECK(test_writes == writes + 1);
      CHECK(r.ui.options_.stage() == Options::STAGE_EDITING);
      CHECK(r.ui.options_.Value(setting) == (setting == OPTION_PITCH
          ? int(r.settings.pitch_scale()) : setting == OPTION_DELAY
          ? int(r.settings.delay_mode()) : setting == OPTION_REVERB
          ? r.settings.clouds_style() : setting == OPTION_PROCESSOR
          ? r.settings.spectral() : setting == OPTION_SYNTH_START
          ? r.settings.self_play_on_request() : setting == OPTION_INTERVALS
          ? r.settings.random_intervals() : setting == OPTION_FROZEN
          ? r.settings.evolving_feedback() : setting == OPTION_SYNTH
          ? r.settings.plaits_synth() : setting == OPTION_ATTACKS
          ? int(r.settings.attack_mode())
          : int(r.settings.output_limiter_mode())));
      r.Tick(5000);
      CHECK(test_writes == writes + 1);
      CHECK(r.ui.options_.active());
    }
  }
  // A leave button keeps a pending change and waits for its one settled save.
  for (int button = 0; button < SWITCH_LAST; ++button) {
    if (button == SWITCH_ASSIGN) continue;
    Rig r;
    r.Enter();
    const uint32_t writes = test_writes;
    r.reverb = .9f;
    r.Tick(10);
    r.Down(Switch(button));
    r.Up(Switch(button));
    CHECK(!r.ui.options_.active());
    r.Tick(1000);
    CHECK(!r.ui.options_.active());
    CHECK(r.settings.attack_mode() == ATTACK_MODE_SNAP_AND_TRIGGER);
    CHECK(test_writes == writes + 1);
  }
}

void TestOptionsSeed() {
  test_checker.Start("TestOptionsSeed");
  Rig saved;
  saved.settings.set_pitch_scale(PITCH_SCALE_FIFTHS);
  saved.Enter();
  saved.Down(SWITCH_SEED);
  saved.Up(SWITCH_SEED);
  CHECK(saved.ui.options_.pitch_preview() == PITCH_SCALE_MAJOR);
  CHECK(!saved.ui.options_.qualified_[0]);
  Rig r;
  r.Enter();
  r.feedback = .9f;
  r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_FIFTHS);
  const bool seed_state = r.ui.seed_button_state_;
  const bool seed_locked = r.ui.seed_button_locked_;
  for (int i = 0; i < 8; ++i) {
    test_down[SWITCH_SEED] = true;
    r.Tick(8, false);
    CHECK(!r.ui.queue_.available());
    test_down[SWITCH_SEED] = false;
    r.Tick(8, false);
    CHECK(!r.ui.queue_.available());
    r.ui.DoEvents();
    CHECK(r.ui.seed_button_state_ == seed_state);
    CHECK(r.ui.seed_button_locked_ == seed_locked);
    CHECK(r.ui.options_.pitch_preview() ==
        PitchScale(PITCH_SCALE_FIFTHS + (i + 1) % 8));
    CHECK(r.ui.options_.active());
  }
  r.Down(SWITCH_SEED);
  r.Tick(600);
  r.Up(SWITCH_SEED);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_FIFTHS);
  r.Tick(1001);
  CHECK(r.settings.pitch_scale() == PITCH_SCALE_FIFTHS);
}

void TestOptionsZonesAndSaveBatch() {
  test_checker.Start("TestOptionsZonesAndSaveBatch");
  Rig r;
  r.Enter();
  // Pitch thirds, including hysteresis around both boundaries.
  r.feedback = .1f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_OFF);
  r.feedback = .35f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_OFF);
  r.feedback = .38f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_CHROMATIC);
  r.feedback = .68f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_CHROMATIC);
  r.feedback = .71f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_FIFTHS);
  r.feedback = .65f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_FIFTHS);
  r.feedback = .62f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_CHROMATIC);
  r.feedback = .29f; r.Tick(1);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_OFF);
  // Halves retain a value inside the midpoint dead band; two knobs
  // settle in one batch.
  {
    Rig halves;
    halves.EnterPage(1);
    halves.pot = .9f; halves.Tick(1);
    halves.pot = .5f; halves.Tick(1);
    CHECK(halves.ui.options_.clouds_preview());
    halves.pot = .46f; halves.Tick(1);
    CHECK(!halves.ui.options_.clouds_preview());
    halves.pot = .54f; halves.Tick(1);
    const uint32_t writes = test_writes;
    halves.reverb = .9f; halves.Tick(1001);
    CHECK(test_writes == writes + 1);
    CHECK(halves.settings.clouds_style());
    CHECK(halves.settings.evolving_feedback());
    Settings loaded;
    loaded.Init(false);
    CHECK(loaded.clouds_style() && loaded.evolving_feedback());
  }
  // Main may be delayed: published batch must stay stable until consumed.
  r.feedback = .9f; r.Tick(1001, false);
  CHECK(r.ui.options_.commit_requested());
  r.feedback = .1f; r.Tick(2000, false);
  CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_FIFTHS);
  // Exit remains responsive even if main is delayed by a flash operation.
  test_down[SWITCH_MEDIA] = true;
  r.Tick(8, false);
  CHECK(r.ui.options_.stage() == Options::STAGE_DRAIN);
  test_down[SWITCH_MEDIA] = false;
  r.Tick(8, false);
  r.ui.DoEvents();
  CHECK(r.settings.pitch_scale() == PITCH_SCALE_FIFTHS);
  r.Tick(10);
  CHECK(!r.ui.options_.active());
}

void TestOptionsPalette() {
  test_checker.Start("TestOptionsPalette");
  for (size_t i = 1; i < kNumOptionValues; ++i) {
    const OptionValue& v = kOptionValues[i];
    CHECK(v.color.r || v.color.g || v.color.b);
    if (v.value == 0) {
      CHECK(v.color.r == 0 && v.color.g == 255 &&
          v.color.b == 0 && v.pulses == 0);
    }
    CHECK(v.color.b == 0);
    CHECK((v.color.r == 0 && v.color.g == 255) ||
        (v.color.r == 255 && (v.color.g == 0 || v.color.g == 255)));
    for (size_t j = i + 1; j < kNumOptionValues; ++j) {
      const OptionValue& w = kOptionValues[j];
      if (v.setting == w.setting) {
        CHECK(v.color.r != w.color.r || v.color.g != w.color.g ||
            v.color.b != w.color.b || v.pulses != w.pulses);
      }
    }
  }
  // Renderer consumes the table at every phase of its pulse period.
  Rig r;
  r.Enter();
  for (size_t i = 1; i < kNumOptionValues; ++i) {
    const OptionValue& v = kOptionValues[i];
    r.ui.options_.selected_ = v.setting;
    switch (v.setting) {
      case OPTION_PITCH: r.ui.options_.pitch_preview_ = PitchScale(v.value);
        break;
      case OPTION_REVERB: r.ui.options_.clouds_preview_ = v.value; break;
      case OPTION_DELAY: r.ui.options_.delay_preview_ = DelayMode(v.value);
        break;
      case OPTION_PROCESSOR: r.ui.options_.spectral_preview_ = v.value; break;
      case OPTION_SYNTH_START:
        r.ui.options_.manual_start_preview_ = v.value;
        break;
      case OPTION_INTERVALS: r.ui.options_.intervals_preview_ = v.value; break;
      case OPTION_SYNTH: r.ui.options_.plaits_preview_ = v.value; break;
      case OPTION_LIMITER:
        r.ui.options_.limiter_preview_ = OutputLimiterMode(v.value);
        break;
      case OPTION_FROZEN: r.ui.options_.evolving_preview_ = v.value; break;
      case OPTION_ATTACKS:
        r.ui.options_.attack_preview_ = AttackMode(v.value);
        break;
      default: break;
    }
    const OptionValue& indication = r.ui.options_.indication();
    CHECK(&indication == &v);
    CHECK(indication.setting == v.setting && indication.value == v.value);
    CHECK(indication.color.r == v.color.r && indication.color.g == v.color.g &&
        indication.color.b == v.color.b && indication.pulses == v.pulses);
    for (int tick = 0; tick < 2048; ++tick) {
      stmlib::system_clock.Tick();
      r.ui.UpdateLeds();
      // Blinks start with the setting they show.
      const uint32_t now = stmlib::system_clock.milliseconds() -
          r.ui.options_.indication_time();
      const uint32_t phase = now % 2048;
      const bool lit = v.pulses == 0 ||
          (v.pulses == 1 && now % 1024 < 512) ||
          (v.pulses == 2 && now % 256 < 128) ||
          (v.pulses == 3 && (phase < 128 || (phase >= 256 && phase < 384)));
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == (lit ? v.color.r : 0));
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == (lit ? v.color.g : 0));
      CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == (lit ? v.color.b : 0));
    }
  }
}

void TestOptionsConfirmations() {
  test_checker.Start("TestOptionsConfirmations");
  Rig r;
  r.Enter();
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1] == 255);
  const uint32_t entry = r.ui.options_flash_time_;
  const uint32_t writes = test_writes;
  r.reverb = .9f;  // Ends the preview.
  r.Tick(1010);
  CHECK(r.ui.options_.preview_knob() == -1);
  CHECK(test_writes == writes + 1);
  CHECK(r.ui.options_flash_time_ != entry);
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1] == 255);
  const uint32_t saved = r.ui.options_flash_time_;
  r.Tick(1100);
  CHECK(r.ui.options_flash_time_ == saved);  // No write, no confirmation.
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1] < 255);
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_3] == 0);
  r.Down(SWITCH_FREEZE);
  CHECK(r.ui.options_flash_time_ != saved);
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1] == 255);
  r.Up(SWITCH_FREEZE);
  r.Tick(100);
  CHECK(stmlib::system_clock.milliseconds() - r.ui.options_flash_time_ >= 100);
  CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1] < 255);
  // The last page flashes all three.
  Rig last;
  last.EnterLimiter();
  last.reverb = .9f;
  last.Tick(1010);
  for (int i = LED_ASSIGN_1; i < LED_ASSIGN_1 + 3; ++i) {
    CHECK(last.ui.leds_.intensity_[i] == 255);
  }
  CHECK(last.settings.output_limiter_mode() == OUTPUT_LIMITER_OFF);
}

// Opening a page shows each stored value in turn, left to right, with its
// knob lit; touching a knob ends it.
void TestOptionsPreview() {
  test_checker.Start("TestOptionsPreview");
  Rig r;
  r.settings.set_delay_mode(DELAY_MODE_WIDE_DETENT);
  r.settings.set_clouds_style(true);
  r.settings.set_evolving_feedback(false);
  r.ui.LoadOptions();
  r.EnterPage(1);
  const OptionSetting order[3] = { OPTION_DELAY, OPTION_REVERB, OPTION_FROZEN };
  const int values[3] = { DELAY_MODE_WIDE_DETENT, 1, 0 };
  for (int knob = 0; knob < 3; ++knob) {
    CHECK(r.ui.options_.preview_knob() == knob);
    const OptionValue& v = r.ui.options_.indication();
    CHECK(v.setting == order[knob] && v.value == values[knob]);
    r.Tick(1);
    CHECK(r.ui.leds_.intensity_[LED_ASSIGN_1 + knob] == 255);
    r.Tick(2048 - 1);
  }
  CHECK(r.ui.options_.preview_knob() == -1);
  CHECK(r.ui.options_.indication().setting == OPTION_DELAY);
  // Reserved knobs are skipped; a knob ends the preview at once.
  r.Tap();
  r.Tap();
  CHECK(r.ui.options_.page() == 3);
  CHECK(r.ui.options_.preview_knob() == 2);
  r.Tap();
  CHECK(r.ui.options_.page() == 0);
  CHECK(r.ui.options_.preview_knob() == 0);
  r.Tick(2048);
  CHECK(r.ui.options_.preview_knob() == 1);
  r.reverb = .9f;
  r.Tick(1);
  CHECK(r.ui.options_.preview_knob() == -1);
  CHECK(r.ui.options_.indication().setting == OPTION_ATTACKS);
  const uint32_t writes = test_writes;
  r.Tick(1100);
  CHECK(test_writes == writes + 1);
  // Each shown value starts its blink pattern, even on the same setting.
  Rig phase;
  phase.Enter();
  phase.Tick(500);
  for (int page = 0; page < kNumOptionPages; ++page) {
    phase.Tap();
    CHECK(stmlib::system_clock.milliseconds() -
        phase.ui.options_.indication_time() <= 8);
    phase.Tick(500);
  }
  // The preview alone never writes.
  Rig idle;
  const uint32_t idle_writes = test_writes;
  idle.Enter();
  idle.Tick(10000);
  CHECK(test_writes == idle_writes);
}

bool IsVolatile(bool*) { return false; }
bool IsVolatile(volatile bool*) { return true; }

void TestCommitPublication() {
  test_checker.Start("TestCommitPublication");
  Rig r;
  CHECK(IsVolatile(&r.ui.options_.dirty_));
  CHECK(IsVolatile(&r.ui.options_.commit_));
}

void TestPendingSeed() {
  test_checker.Start("TestPendingSeed");
  for (int hold = 0; hold < 3; ++hold) {
    Rig r;
    r.settings.set_pitch_scale(PITCH_SCALE_FIFTHS);
    r.Enter();
    r.reverb = .9f;
    r.Tick(1001, false);
    CHECK(r.ui.options_.commit_requested());
    test_down[SWITCH_SEED] = true;
    r.Tick(hold ? 600 : 20, false);
    if (hold == 2) r.ui.DoEvents();
    test_down[SWITCH_SEED] = false;
    r.Tick(20, false);
    CHECK(r.ui.options_.pitch_preview() == PITCH_SCALE_FIFTHS);
    r.ui.DoEvents();
    r.Tick(1);
    CHECK(r.ui.options_.pitch_preview() ==
        (hold ? PITCH_SCALE_FIFTHS : PITCH_SCALE_MAJOR));
    r.Tick(1001);
    CHECK(r.settings.pitch_scale() ==
        (hold ? PITCH_SCALE_FIFTHS : PITCH_SCALE_MAJOR));
  }
  // Press before publication; release during it, then delay main long enough
  // that using the consumption time would misclassify this as a long hold.
  Rig before;
  before.settings.set_pitch_scale(PITCH_SCALE_FIFTHS);
  before.Enter();
  before.reverb = .9f;
  before.Tick(990, false);
  test_down[SWITCH_SEED] = true;
  before.Tick(20, false);
  CHECK(before.ui.options_.commit_requested());
  test_down[SWITCH_SEED] = false;
  before.Tick(620, false);
  before.ui.DoEvents();
  before.Tick(1);
  CHECK(before.ui.options_.pitch_preview() == PITCH_SCALE_MAJOR);
  before.Tick(1001);
  CHECK(before.settings.pitch_scale() == PITCH_SCALE_MAJOR);
}

void TestPendingExitButtons() {
  test_checker.Start("TestPendingExitButtons");
  Rig r;
  r.Enter();
  const uint32_t writes = test_writes;
  r.reverb = .9f;
  r.Tick(10);
  r.Down(SWITCH_FREEZE);
  r.Up(SWITCH_FREEZE);
  CHECK(!r.ui.options_.active());
  r.ui.set_overwrite_reserve(1.0f);
  r.Tick(1);
  const Leds::Color normal = r.ui.media_palette_[r.settings.state().media];
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == normal.r);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == normal.g);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == normal.b);
  const bool frozen = r.ui.freeze_button_state_;
  r.Down(SWITCH_FREEZE);
  r.Up(SWITCH_FREEZE);
  CHECK(r.ui.freeze_button_state_ != frozen);
  r.Tick(1001);
  CHECK(r.settings.attack_mode() == ATTACK_MODE_SNAP_AND_TRIGGER);
  CHECK(test_writes == writes + 1);
}

void CustomProfile(uint8_t* b, int id, int page, int amount, int action) {
  for (size_t i = 0; i < kProfileSize; ++i) {
    b[i] = kProfileBlock[i];
  }
  b[5] = id ? 1 : 0;
  b[6] = id ? 6 : 0;
  b[16] = id;
  b[17] = PROFILE_ADJUSTABLE;
  b[18] = id == FEATURE_SYNTH_START ? 1 : 0;
  b[19] = page;
  b[20] = amount;
  b[21] = action;
  const size_t size = 16 + b[6];
  const uint32_t crc = Profile::Crc32(b, size);
  for (int i = 0; i < 4; ++i) {
    b[size + i] = crc >> (8 * i);
  }
}

void ApplyCustomProfile(Rig* r, const uint8_t* b) {
  CHECK(r->settings.Init(false, b));
  CHECK(!r->settings.profile_locked());
  r->ui.Init(&r->settings, &r->gain, &r->calibration, &r->backup);
  r->ui.LoadOptions();
  r->Tick(40);
}

void TestProfilePages() {
  test_checker.Start("TestProfilePages");
  uint8_t b[kProfileSize] = { 0 };
  Rig invalid;
  invalid.settings.Init(false, b);
  invalid.ui.Init(&invalid.settings, &invalid.gain,
      &invalid.calibration, &invalid.backup);
  invalid.ui.LoadOptions();
  invalid.ui.ShowStorageStatus();
  CHECK(invalid.ui.mode_ == Ui::MODE_NORMAL);
  for (int id = FEATURE_PITCH; id <= FEATURE_INTERVALS; ++id) {
    for (int slot = 1; slot <= 12; ++slot) {
      Rig r;
      CustomProfile(b, id, slot, 0, 0);
      ApplyCustomProfile(&r, b);
      r.Enter();
      CHECK(r.ui.options_.page() == (slot - 1) / 3);
      CHECK(r.ui.options_.preview_knob() == (slot - 1) % 3);
      CHECK(r.ui.options_.selected_ == OptionSetting(id));
      r.Tap();
      CHECK(r.ui.options_.page() == (slot - 1) / 3);
      r.ui.options_.pitch_preview_ = PITCH_SCALE_FIFTHS;
      CHECK(r.ui.options_.seed_allowed() == (id == FEATURE_PITCH));
    }
  }
  Rig empty;
  CustomProfile(b, 0, 0, 0, 0);
  ApplyCustomProfile(&empty, b);
  const uint32_t writes = test_writes;
  const State before = empty.settings.state();
  empty.Down(SWITCH_ASSIGN);
  empty.Tick(3100);
  empty.Up(SWITCH_ASSIGN);
  CHECK(test_writes == writes);
  CHECK(!memcmp(&before, &empty.settings.state(), sizeof(before)));
  CHECK(empty.ui.options_.stage() == Options::STAGE_INACTIVE);
  CHECK(empty.ui.options_.preview_knob() == -1);
}

void TestProfileActions() {
  test_checker.Start("TestProfileActions");
  uint8_t b[kProfileSize];
  const int features[] = { FEATURE_MONOPHONIC, FEATURE_TRIM, FEATURE_SYNTH_START };
  for (int i = 0; i < 3; ++i) {
    for (int slot = 1; slot <= 3; ++slot) {
      for (int patched = 0; patched < 2; ++patched) {
        Rig r;
        CustomProfile(b, features[i], 0, 0, slot);
        ApplyCustomProfile(&r, b);
        r.settings.set_self_play_on_request(true);
        r.ui.set_no_input(!patched);
        r.settings.mutable_state()->gain = 123;
        r.Down(SWITCH_MEDIA);
        if (slot == 3) {
          r.Tick(1510);
        } else {
          r.Down(slot == 1 ? SWITCH_SEED : SWITCH_FREEZE);
        }
        // Only the synth needs empty inputs; a handled hold keeps the gain.
        const bool handled = features[i] != FEATURE_SYNTH_START || !patched;
        if (features[i] == FEATURE_MONOPHONIC) {
          CHECK(r.settings.state().monophonic_mode);
        } else if (features[i] == FEATURE_TRIM) {
          CHECK(r.ui.trim_flag_);
        } else {
          CHECK(r.ui.self_play_requested() == !patched);
        }
        const bool reset = slot == 3 && !handled;
        CHECK(r.gain.restarts == (reset ? 1 : 0));
        CHECK(r.settings.state().gain == (reset ? 0 : 123));
      }
    }
  }
  Rig off;
  CustomProfile(b, 0, 0, 0, 0);
  ApplyCustomProfile(&off, b);
  off.Down(SWITCH_MEDIA);
  off.Down(SWITCH_SEED);
  CHECK(!off.settings.state().monophonic_mode);
  off.Up(SWITCH_SEED);
  off.Down(SWITCH_FREEZE);
  CHECK(!off.ui.trim_flag_);
}

void TestProfileAmounts() {
  test_checker.Start("TestProfileAmounts");
  uint8_t b[kProfileSize];
  for (int id = FEATURE_WIDTH; id <= FEATURE_DIFFUSION; ++id) {
    for (int slot = 1; slot <= 2; ++slot) {
      Rig r;
      CustomProfile(b, id, 0, slot, 0);
      ApplyCustomProfile(&r, b);
      r.Down(SWITCH_MEDIA);
      if (slot == 1) {
        r.pot = 1.0f;
      } else {
        r.reverb = 1.0f;
      }
      r.Tick(40);
      if (id == FEATURE_WIDTH) {
        CHECK(r.settings.stereo_width() == 255);
        CHECK(r.settings.diffusion() == 0.0f);
      } else {
        CHECK(r.settings.state().diffusion == 255);
        CHECK(r.settings.stereo_width() == 0);
      }
    }
  }
}

void SetFixed(Rig* r, ProfileFeature feature, uint8_t value) {
  uint8_t b[kProfileSize];
  CustomProfile(b, feature, 0, 0, 0);
  b[17] = PROFILE_FIXED;
  b[18] = value;
  const uint32_t crc = Profile::Crc32(b, 22);
  for (int i = 0; i < 4; ++i) {
    b[22 + i] = crc >> (8 * i);
  }
  ApplyCustomProfile(r, b);
}

void TestBetaOptionsLeds() {
  test_checker.Start("TestBetaOptionsLeds");
  Rig version;
  SetFixed(&version, FEATURE_VERSION, 1);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_B] == 255);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_G] == 128);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_R] == 0);
  version.Tick(100);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_B] == 0);
  version.Tick(100);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_B] == 255);
  version.Tick(150);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_B] == 0);
  version.Tick(150);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_B] == 255);
  version.Tick(300);
  CHECK(version.ui.leds_.intensity_[LED_MEDIA_R] != 0);


}

int main() {
  if (!CHECK(mmap(reinterpret_cast<void*>(0x08100000), 0x100000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
      -1, 0) == reinterpret_cast<void*>(0x08100000))) {
    return 1;
  }
  TestCommitPublication();
  TestPendingSeed();
  TestPendingExitButtons();
  TestOptionsTablesAndLifetime();
  TestOptionsKnobsAndSettling();
  TestOptionsSeed();
  TestOptionsPreview();
  TestOptionsPalette();
  TestOptionsConfirmations();
  TestOptionsZonesAndSaveBatch();
  TestWidthAssignCycle();
  TestWidthGestureAndMeter();
  TestDiffusionGestureAndMeter();
  TestDiffusionStorage();
  TestDiffusionAssignCycle();
  TestPlaybackStorage();
  TestCloudsStorage();
  TestFrozenFeedbackStorage();
  TestAttackStorage();
  TestSynthEngineStorage();
  TestPitchStorage();
  TestSelfPlayStorage();
  TestEngineStorage();
  TestEngineOption();
  TestSynthStartOption();
  TestIntervalsStorage();
  TestIntervalsOption();
  TestSelfPlayHold();
  TestTrimGesture();
  TestMonophonicGesture();
  TestAudioPatchDetection();
  TestFrozenStateChangeGuard();
  TestGateOutputChordPersistence();
  TestOverwriteReserveDisplay();
  TestCorruptCalibrationLocks();
  TestSettingsRollover();
  TestStorageStaleErrorFlag();
  TestStorageFailedErase();
  TestStorageNonBlankSlot();
  TestStorageFailedProgram();
  TestStorageMatchesStmlib();
  TestQualityTapCycle();
  TestManualGainGestureAndMeter();
  TestSeedStartupLatch();
  TestAssignableAmountLeds();
  TestAgcAdjustmentBlink();
  TestRecoveryPreservesCalibration();
  TestStockAssignRouting();
  TestStockAssignAmounts();
  TestAutosaveChordAndDisplay();
  TestSeedLatchGestures();
  TestGainHoldThresholdAndReload();
  TestBackupLedAndRestore();
  TestLimiterLinkedGainAndModeSwitch();
  TestQueueRace();
  TestStorage();
  TestDsp();
  TestProfilePages();
  TestBetaOptionsLeds();
  TestProfileActions();
  TestProfileAmounts();
  return test_checker.failures() ? 1 : 0;
}
