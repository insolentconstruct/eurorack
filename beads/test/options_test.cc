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
unsigned test_writes = 0;
namespace stmlib {

SystemClock system_clock;

}  // namespace stmlib

namespace beads {

bool Leds::Init() {
  Clear();
  return true;
}
void Leds::Clear() { std::fill(intensity_, intensity_ + LED_LAST, 0); }
void Leds::Write() { }

}  // namespace beads

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
STATIC_ASSERT(offsetof(State, padding) == 10, options);

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
    memset(reinterpret_cast<void*>(0x081e0000), 255, 0x20000);
    settings.Init(false);
    ui.Init(&settings, &gain, &calibration, &backup);
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
  void Tap() {
    Down(SWITCH_ASSIGN);
    Up(SWITCH_ASSIGN);
  }
};

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
      unsigned writes = test_writes;
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
  page.Enter();
  page.Down(SWITCH_MEDIA);
  page.Down(SWITCH_SEED);
  page.Up(SWITCH_SEED);
  page.Up(SWITCH_MEDIA);
  CHECK(!page.settings.state().monophonic_mode);
}

void TestUi() {
  test_checker.Start("TestUi");
  Rig r;
  unsigned writes = test_writes;
  r.Down(SWITCH_ASSIGN);
  r.Tick(2992);
  CHECK(!r.ui.options_.active());
  r.Tick(8);
  r.Up(SWITCH_ASSIGN);
  CHECK(r.ui.options_.stage() == Options::STAGE_EDITING);
  // Deliberate movement below qualification refreshes inactivity only.
  r.Tick(4000);
  r.pot = .54f;
  r.Tick(1500);
  CHECK(r.ui.options_.stage() == Options::STAGE_EDITING);
  CHECK(r.ui.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  r.pot = .9f;
  r.Tick(10);
  CHECK(r.ui.output_limiter_mode() == OUTPUT_LIMITER_OFF);
  r.settings.SaveState();
  ++writes;  // Preview must not leak into unrelated saves.
  CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  r.Down(SWITCH_ASSIGN);
  r.Tick(3000, false);
  CHECK(r.ui.options_.commit_requested());
  CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  r.ui.DoEvents();
  r.Up(SWITCH_ASSIGN);
  CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_OFF);
  CHECK(test_writes == writes + 1);
  r.Tick(30);
  r.Enter();
  r.pot = .1f;
  r.Tick(10);
  r.Tick(5000);
  CHECK(r.ui.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  CHECK(test_writes == writes + 2);  // Timeout saves a changed value.
  r.Tick(30);
  r.Enter();
  r.Tick(5010);
  CHECK(!r.ui.options_.active());
  CHECK(test_writes == writes + 2);  // Timeout without a change writes nothing.
  r.Tick(30);
  r.Enter();
  CHECK(r.ui.options_.page() == kLimiterPage);  // Opens on dry/wet.
  r.Tap();
  CHECK(r.ui.options_.page() == 2);
  r.pot = .9f;
  r.Tick(10);
  CHECK(r.ui.output_limiter_mode() == OUTPUT_LIMITER_STOCK);  // Reserved.
  r.Tap();
  r.Tap();
  CHECK(r.ui.options_.page() == kLimiterPage);
  r.pot = .5f;
  r.Tick(10);
  CHECK(r.ui.output_limiter_mode() == OUTPUT_LIMITER_FAST);
  r.Down(SWITCH_MEDIA);
  CHECK(r.calibration.starts == 0);  // Active page consumes stock chords.
  Rig startup(true);
  startup.Tick(3500);
  CHECK(!startup.ui.options_.active());
  Rig chord;
  chord.Down(SWITCH_ASSIGN);
  chord.Down(SWITCH_MEDIA);
  CHECK(chord.calibration.starts == 1);
  puts("gesture/preview/commit: PASS");
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
      r->Tick(5010, false);
    } else {
      r->Tick(1, false);  // After flush check, before options_.active().
    }
  }
}

}  // namespace beads

void TestQueueRace() {
  test_checker.Start("TestQueueRace");
  Rig r;
  r.Enter();
  r.Down(SWITCH_MEDIA);  // Cancellation enters DRAIN with MEDIA down.
  CHECK(r.ui.options_.stage() == Options::STAGE_DRAIN);
  test_down[SWITCH_MEDIA] = false;
  r.Tick(7, false);  // Debounce 0x7f: release; next tick 0xff exits DRAIN.
  CHECK(r.ui.options_.stage() == Options::STAGE_DRAIN);
  uint8_t media = r.settings.state().media;
  unsigned writes = test_writes;
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
  delayed.Tick(5010, false);  // Release, inactivity cancellation, drain exit.
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

void TestStorage() {
  test_checker.Start("TestStorage");
  Settings s;
  memset(reinterpret_cast<void*>(0x081e0000), 255, 0x20000);
  CHECK(!s.Init(false));
  CHECK(s.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  Settings old_record;
  CHECK(old_record.Init(false));
  CHECK(old_record.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  for (int i = 0; i < 256; ++i) {
    s.mutable_state()->padding[0] = i;
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
  State legacy_state = { };
  PersistentData legacy_calibration = { };
  stmlib::ChunkStorage<15, PersistentData, State> legacy;
  CHECK(legacy.Init(&legacy_calibration, &legacy_state));
  CHECK(legacy_state.media == 3 && legacy_state.padding[0] == 0xa2);
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
  r.Enter();
  for (int i = 0; i < 4; ++i) {
    r.Tap();
    CHECK(r.settings.state().assignable_cv_destination == 0);
  }
}

void TestStockAssignAmounts() {
  test_checker.Start("TestStockAssignAmounts");
  for (int i = 0; i < 3; ++i) {
    Rig r;
    const State before = r.settings.state();
    unsigned writes = test_writes;
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
    CHECK(!r.ui.options_.active());
  }
}

void TestAutosaveChordAndDisplay() {
  test_checker.Start("TestAutosaveChordAndDisplay");
  Rig r;
  for (int i = 0; i < 2; ++i) {
    unsigned writes = test_writes;
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
  unsigned writes = test_writes;
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
  unsigned writes = test_writes;
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
  unsigned writes = test_writes;
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
  unsigned writes = test_writes;
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

void TestCorruptCalibrationRewritesOnce() {
  test_checker.Start("TestCorruptCalibrationRewritesOnce");
  Rig r;
  const State defaults = r.settings.state();
  const PersistentData calibration = r.settings.persistent_data();
  r.settings.mutable_state()->media = 0;
  r.settings.SaveState();
  uint8_t* flash = reinterpret_cast<uint8_t*>(0x081e0000);
  flash[8] ^= 1;  // Corrupt CALI payload without changing its checksum.
  unsigned writes = test_writes;
  Settings first;
  CHECK(!first.Init(false));
  CHECK(test_writes > writes);
  CHECK(!memcmp(&first.state(), &defaults, sizeof(State)));
  CHECK(!memcmp(first.persistent_data().adc_calibration,
      calibration.adc_calibration, sizeof(calibration.adc_calibration)));
  writes = test_writes;
  for (int i = 0; i < 3; ++i) {
    Settings next;
    CHECK(next.Init(false));
    CHECK(test_writes == writes);
    CHECK(!memcmp(&next.state(), &defaults, sizeof(State)));
    CHECK(!memcmp(next.persistent_data().adc_calibration,
        calibration.adc_calibration, sizeof(calibration.adc_calibration)));
  }
}

void TestSelfPlayOptions() {
  test_checker.Start("TestSelfPlayOptions");
  Rig r;
  r.Enter();
  r.Tap();
  r.Tap();
  CHECK(r.ui.options_.page() == kSelfPlayPage);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == 255);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == 255);
  r.feedback = 0.8f;
  r.Tick(1);
  CHECK(r.ui.options_.self_play_preview());
  r.feedback = 0.49f;
  r.Tick(1);
  CHECK(r.ui.options_.self_play_preview());
  r.feedback = 0.46f;
  r.Tick(1);
  CHECK(!r.ui.options_.self_play_preview());
  r.feedback = 0.51f;
  r.Tick(1);
  CHECK(!r.ui.options_.self_play_preview());
  r.feedback = 0.8f;
  for (int i = 0; i < 1200; ++i) {
    r.Tick(1);
    bool pulse = stmlib::system_clock.milliseconds() % 1200 < 120;
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == (pulse ? 255 : 0));
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == 0);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == (pulse ? 255 : 0));
  }
  CHECK(!r.settings.self_play_on_request());
  r.Down(SWITCH_ASSIGN);
  r.Tick(3000);
  r.Up(SWITCH_ASSIGN);
  CHECK(r.settings.self_play_on_request());
  CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.self_play_on_request());
  r.Tick(30);
  r.Enter();
  r.Tap();
  r.Tap();
  r.feedback = 0.1f;
  r.Tick(1);
  CHECK(!r.ui.options_.self_play_preview());
  r.Down(SWITCH_MEDIA);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.self_play_on_request());
  CHECK(r.gain.restarts == 0 && !r.ui.self_play_requested());
  r.Tick(30);
  r.Enter();
  r.Tap();
  r.Tap();
  r.feedback = 0.9f;
  r.Tick(1);
  r.feedback = 0.1f;
  r.Tick(5001);
  CHECK(!r.settings.self_play_on_request());
  CHECK(loaded.Init(false) && !loaded.self_play_on_request());
}

void TestSelfPlayStorage() {
  test_checker.Start("TestSelfPlayStorage");
  Rig r;
  for (int i = 0; i < 256; ++i) {
    r.settings.mutable_state()->padding[1] = i;
    CHECK(r.settings.self_play_on_request() == (i == 0xa1));
    r.settings.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.self_play_on_request() == (i == 0xa1));
    CHECK(loaded.state().padding[1] == (i == 0xa1 ? 0xa1 : 0));
  }
  r.settings.set_self_play_on_request(true);
  r.settings.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
  r.settings.SaveState();
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.self_play_on_request());
  CHECK(loaded.output_limiter_mode() == OUTPUT_LIMITER_OFF);
  State legacy_state = { };
  PersistentData calibration = { };
  stmlib::ChunkStorage<15, PersistentData, State> legacy;
  CHECK(legacy.Init(&calibration, &legacy_state));
  memset(legacy_state.padding, 0, sizeof(legacy_state.padding));
  legacy.SaveState();
  CHECK(loaded.Init(false) && !loaded.self_play_on_request());
  loaded.mutable_state()->padding[1] = 0xff;
  loaded.chunk_storage_.SaveState();
  CHECK(loaded.Init(false));
  CHECK(loaded.state().padding[1] == 0);
}

void TestCloudsStorage() {
  test_checker.Start("TestCloudsStorage");
  Rig r;
  for (int value = 0; value < 256; ++value) {
    r.settings.mutable_state()->padding[3] = value;
    CHECK(r.settings.clouds_style() == (value == 0xa2));
    r.settings.mutable_state()->padding[2] = 0xa6;
    r.settings.mutable_state()->padding[13] = 0xff;
    r.settings.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.clouds_style() == (value == 0xa2));
    CHECK(loaded.state().padding[3] == (value == 0xa2 ? 0xa2 : 0));
    CHECK(loaded.state().padding[2] == 0xa6);
    CHECK(loaded.pitch_scale() == PITCH_SCALE_PELOG);
    CHECK(loaded.state().padding[13] == 0);
    // Bypass SaveState normalization to exercise load normalization too.
    loaded.mutable_state()->padding[3] = value;
    loaded.chunk_storage_.SaveState();
    CHECK(loaded.Init(false));
    CHECK(loaded.state().padding[3] == (value == 0xa2 ? 0xa2 : 0));
    CHECK(r.settings.Init(false));
  }
}

void TestCloudsOptions() {
  test_checker.Start("TestCloudsOptions");
  Rig r;
  r.Enter();
  CHECK(r.ui.options_.page() == kLimiterPage);
  r.reverb = 0.55f;
  r.Tick(1);
  CHECK(!r.ui.clouds_style());  // Pickup still locked.
  r.reverb = 0.8f;
  r.Tick(1);
  CHECK(r.ui.clouds_style());
  r.reverb = 0.49f;
  r.Tick(1);
  CHECK(r.ui.clouds_style());
  r.reverb = 0.46f;
  r.Tick(1);
  CHECK(!r.ui.clouds_style());
  r.reverb = 0.51f;
  r.Tick(1);
  CHECK(!r.ui.clouds_style());
  r.reverb = 0.9f;
  for (int i = 0; i < 1200; ++i) {
    r.Tick(1);
    const bool pulse = stmlib::system_clock.milliseconds() % 1200 < 120;
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == 0);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == (pulse ? 255 : 0));
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == (pulse ? 255 : 0));
  }
  CHECK(!r.settings.clouds_style());
  r.pot = 0.9f;
  r.Tick(1);
  CHECK(!r.ui.options_.reverb_selected());
  CHECK(r.ui.options_.preview() == OUTPUT_LIMITER_OFF);
  CHECK(r.ui.clouds_style());
  r.pot = 0.1f;
  r.Tick(1);
  CHECK(r.ui.options_.preview() == OUTPUT_LIMITER_STOCK);
  CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == 255);
  r.Down(SWITCH_ASSIGN);
  r.Tick(3000);
  r.Up(SWITCH_ASSIGN);
  CHECK(r.settings.clouds_style());
  CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_STOCK);
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.clouds_style());
  r.Tick(30);
  r.Enter();
  r.reverb = 0.1f;
  r.Tick(1);
  CHECK(!r.ui.clouds_style());
  r.Down(SWITCH_MEDIA);
  r.Up(SWITCH_MEDIA);
  CHECK(r.settings.clouds_style() && r.ui.clouds_style());
  r.Tick(30);
  r.Enter();
  r.reverb = 0.9f;
  r.Tick(1);
  r.reverb = 0.1f;
  r.Tick(5001);
  CHECK(!r.settings.clouds_style());
  CHECK(loaded.Init(false) && !loaded.clouds_style());
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
        unsigned writes = test_writes;
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

void TestPitchStorage() {
  test_checker.Start("TestPitchStorage");
  Rig r;
  r.settings.set_output_limiter_mode(OUTPUT_LIMITER_OFF);
  r.settings.set_self_play_on_request(true);
  for (int i = 0; i < 256; ++i) {
    const PitchScale expected = i >= 0xa1 && i <= 0xa8
        ? PitchScale(i - 0xa0) : PITCH_SCALE_OFF;
    r.settings.mutable_state()->padding[2] = i;
    CHECK(r.settings.pitch_scale() == expected);
    // Read unnormalized data from flash, then save it canonically.
    r.settings.chunk_storage_.SaveState();
    Settings loaded;
    CHECK(loaded.Init(false));
    CHECK(loaded.pitch_scale() == expected);
    CHECK(loaded.state().padding[2] == (expected ? i : 0));
    loaded.mutable_state()->padding[13] = 0xff;
    loaded.SaveState();
    CHECK(r.settings.Init(false));
    CHECK(r.settings.pitch_scale() == expected);
    CHECK(r.settings.output_limiter_mode() == OUTPUT_LIMITER_OFF);
    CHECK(r.settings.self_play_on_request());
    CHECK(r.settings.state().padding[13] == 0);
  }
}

const uint8_t kTestScaleColors[7][3] = {
  { 255, 0, 0 }, { 255, 96, 0 }, { 255, 255, 0 }, { 0, 255, 255 },
  { 0, 96, 255 }, { 128, 0, 255 }, { 255, 0, 128 }
};

void TestPitchOptions() {
  test_checker.Start("TestPitchOptions");
  Rig r;
  r.Enter();
  r.Tap();
  CHECK(r.ui.options_.page() == kPitchScalePage);
  r.feedback = 0.58f;
  r.Tick(1);
  CHECK(r.ui.pitch_scale() == PITCH_SCALE_OFF);
  r.feedback = 0.9f;
  r.Tick(1);
  CHECK(r.ui.pitch_scale() == PITCH_SCALE_FIFTHS);
  const bool seed = r.ui.seed_button_state_;
  const bool locked = r.ui.seed_button_locked_;
  const unsigned writes = test_writes;
  for (int i = 0; i < 7; ++i) {
    CHECK(r.ui.pitch_scale() == PitchScale(PITCH_SCALE_FIFTHS + i));
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_R] == kTestScaleColors[i][0]);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_G] == kTestScaleColors[i][1]);
    CHECK(r.ui.leds_.intensity_[LED_MEDIA_B] == kTestScaleColors[i][2]);
    test_down[SWITCH_SEED] = true;
    r.Tick(8, false);
    CHECK(!r.ui.queue_.available());
    test_down[SWITCH_SEED] = false;
    r.Tick(8, false);
    CHECK(!r.ui.queue_.available());
    r.ui.DoEvents();
    CHECK(r.ui.seed_button_state_ == seed);
    CHECK(r.ui.seed_button_locked_ == locked);
    CHECK(!r.ui.seed_button_state_ || seed);
    CHECK(test_writes == writes);
  }
  CHECK(r.ui.pitch_scale() == PITCH_SCALE_FIFTHS);
  r.Down(SWITCH_SEED);
  r.Tick(5100);
  r.Up(SWITCH_SEED);
  CHECK(r.ui.options_.stage() == Options::STAGE_EDITING);
  CHECK(r.ui.pitch_scale() == PITCH_SCALE_FIFTHS);
  r.feedback = 0.65f;
  r.Tick(1);
  CHECK(r.ui.pitch_scale() == PITCH_SCALE_FIFTHS);
  r.feedback = 0.63f;
  r.Tick(1);
  CHECK(r.ui.pitch_scale() == PITCH_SCALE_CHROMATIC);
  r.feedback = 0.34f;
  r.Tick(1);
  CHECK(r.ui.pitch_scale() == PITCH_SCALE_CHROMATIC);
  r.feedback = 0.30f;
  r.Tick(1);
  CHECK(r.ui.pitch_scale() == PITCH_SCALE_OFF);
  r.feedback = 0.1f;
  r.Tick(1);
  CHECK(r.ui.pitch_scale() == PITCH_SCALE_OFF);
  r.feedback = 0.9f;
  r.Tick(1);
  r.Down(SWITCH_SEED);
  r.Up(SWITCH_SEED);
  CHECK(r.ui.pitch_scale() == PITCH_SCALE_MAJOR);
  r.Tick(5001);
  CHECK(r.settings.pitch_scale() == PITCH_SCALE_MAJOR);
  CHECK(test_writes == writes + 1);
  Settings loaded;
  CHECK(loaded.Init(false) && loaded.pitch_scale() == PITCH_SCALE_MAJOR);
  r.Tick(30);
  r.Enter();
  r.Tap();
  r.feedback = 0.1f;
  r.Tick(1);
  CHECK(r.ui.pitch_scale() == PITCH_SCALE_OFF);
  r.Down(SWITCH_MEDIA);
  r.Up(SWITCH_MEDIA);
  CHECK(r.ui.pitch_scale() == PITCH_SCALE_MAJOR);
  CHECK(r.settings.state().media == 3);
  r.Tick(30);
  r.Enter();
  r.Tap();
  r.Down(SWITCH_SEED);
  r.Down(SWITCH_ASSIGN);
  r.Up(SWITCH_SEED);
  r.Up(SWITCH_ASSIGN);
  CHECK(r.settings.pitch_scale() == PITCH_SCALE_MAJOR);
  CHECK(!r.settings.state().gate_output_mode);
}

void TestPitchSeedCancelAndCommit() {
  test_checker.Start("TestPitchSeedCancelAndCommit");
  Rig r;
  r.Enter();
  r.Tap();
  r.feedback = 0.9f;
  r.Tick(1);
  r.Down(SWITCH_SEED);
  r.feedback = 0.1f;
  r.Tick(1);
  r.Up(SWITCH_SEED);
  r.feedback = 0.9f;
  r.Tick(1);
  CHECK(r.ui.pitch_scale() == PITCH_SCALE_FIFTHS);
  r.Down(SWITCH_SEED);
  r.Up(SWITCH_SEED);
  CHECK(r.ui.pitch_scale() == PITCH_SCALE_MAJOR);
  r.feedback = 0.1f;
  r.Tick(1);
  r.feedback = 0.9f;
  r.Tick(1);
  CHECK(r.ui.pitch_scale() == PITCH_SCALE_MAJOR);
  const unsigned writes = test_writes;
  r.Down(SWITCH_ASSIGN);
  r.Tick(3000);
  r.Up(SWITCH_ASSIGN);
  CHECK(r.settings.pitch_scale() == PITCH_SCALE_MAJOR);
  CHECK(test_writes == writes + 1);
}

int main() {
  if (!CHECK(mmap(reinterpret_cast<void*>(0x081e0000), 0x20000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
      -1, 0) == reinterpret_cast<void*>(0x081e0000))) {
    return 1;
  }
  TestCloudsStorage();
  TestCloudsOptions();
  TestSelfPlayOptions();
  TestPitchStorage();
  TestPitchOptions();
  TestPitchSeedCancelAndCommit();
  TestSelfPlayStorage();
  TestSelfPlayHold();
  TestMonophonicGesture();
  TestAudioPatchDetection();
  TestFrozenStateChangeGuard();
  TestGateOutputChordPersistence();
  TestOverwriteReserveDisplay();
  TestCorruptCalibrationRewritesOnce();
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
  TestUi();
  TestDsp();
  return test_checker.failures() ? 1 : 0;
}
